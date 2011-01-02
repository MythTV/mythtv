//To Do
//support missing audio frames
//support analyze-only mode

// C headers
#include <cstdlib>
#include <cstdio>

// POSIC headers
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"
#include "mpeg2fix.h"

#include <QList>
#include <Q3PtrList>
#include <QFileInfo>

#ifdef USING_MINGW
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define ATTR_ALIGN(align) __attribute__ ((__aligned__ (align)))

#define SHOW_MSG(msg) (msg & print_verbose_messages)

static void *my_malloc(unsigned size, mpeg2_alloc_t reason)
{
    (void)reason;
    char * buf;

    if (size)
    {
        buf = (char *) malloc (size + 63 + sizeof (void **));
        if (buf)
        {
            char * align_buf;
            memset(buf, 0, size + 63 + sizeof (void **));
            align_buf = buf + 63 + sizeof (void **);
            align_buf -= (long)align_buf & 63;
            *(((void **)align_buf) - 1) = buf;
            return align_buf;
        }
    }

    return NULL;
}

static void my_av_print(void *ptr, int level, const char* fmt, va_list vl)
{
    (void) ptr;

    static QString full_line("");
    char str[256];

    if (level > AV_LOG_INFO)
        return;
    vsprintf(str, fmt, vl);

    full_line += QString(str);
    if (full_line.endsWith("\n"))
    {
        full_line.truncate(full_line.length() - 1);
        VERBOSE(MPF_IMPORTANT, full_line);
        full_line = QString("");
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
    QString msg;
    return(msg.sprintf("%s%02d:%02d:%02d.%03d", (is_neg) ? "-" : "",
                (unsigned int)(pts / 90000.) / 3600,
                ((unsigned int)(pts / 90000.) % 3600) / 60,
                ((unsigned int)(pts / 90000.) % 3600) % 60,
                (((unsigned int)(pts / 90.) % 3600000) % 60000) % 1000));
}

PTSOffsetQueue::PTSOffsetQueue(int vidid, QList<int> keys, int64_t initPTS)
{
    QList<int>::iterator it;
    poq_idx_t idx;
    vid_id = vidid;
    keyList = keys;
    keyList.append(vid_id);

    idx.newPTS = initPTS;
    idx.pos_pts = 0;
    idx.framenum = 0;
    idx.type = 0;

    for (it = keyList.begin(); it != keyList.end(); ++it)
        offset[*it].push_back(idx);
}

int64_t PTSOffsetQueue::Get(int idx, AVPacket *pkt)
{
    QList<poq_idx_t>::iterator it;
    int64_t value = offset[idx].first().newPTS;
    bool done = false;

    if (!pkt)
        return value;

    //Be aware: the key for offset can be either a file position OR a PTS
    //The type is defined by type (0==PTS, 1==Pos)
    while (offset[idx].count() > 1 && !done)
    {
        it = ++offset[idx].begin();
        if ((((*it).type == 0) && (pkt->pts >= (*it).pos_pts) /* PTS type */) ||
            (((*it).type == 1) /* Pos type */ &&
             ((pkt->pos >= (*it).pos_pts) || (pkt->duration > (*it).framenum))))
        {
            offset[idx].pop_front();
            value = offset[idx].first().newPTS;
        }
        else
            done = true;
    }
    return value;
}

void PTSOffsetQueue::SetNextPTS(int64_t newPTS, int64_t atPTS)
{
    QList<int>::iterator it;
    poq_idx_t idx;

    idx.newPTS = newPTS;
    idx.pos_pts = atPTS;
    idx.type = 0;
    idx.framenum = 0;

    for (it = keyList.begin(); it != keyList.end(); ++it)
        offset[*it].push_back(idx);
}

void PTSOffsetQueue::SetNextPos(int64_t newPTS, AVPacket &pkt)
{
    QList<int>::iterator it;
    int64_t delta = MPEG2fixup::diff2x33(newPTS, offset[vid_id].last().newPTS);
    poq_idx_t idx;

    idx.pos_pts = pkt.pos;
    idx.framenum = pkt.duration;
    idx.type = 1;

    VERBOSE(MPF_FRAME, QString("Offset %1 -> %2 (%3) at %4")
            .arg(PtsTime(offset[vid_id].last().newPTS))
            .arg(PtsTime(newPTS)).arg(PtsTime(delta)).arg(pkt.pos));
    for (it = keyList.begin(); it != keyList.end(); ++it)
    {
        idx.newPTS = newPTS;
        offset[*it].push_back(idx);
        idx.newPTS = delta;
        orig[*it].push_back(idx);
    }
}

int64_t PTSOffsetQueue::UpdateOrigPTS(int idx, int64_t &origPTS, AVPacket &pkt)
{
    int64_t delta = 0;
    QList<poq_idx_t> *dltaList = &orig[idx];
    while (dltaList->count() && 
           (pkt.pos     >= dltaList->first().pos_pts ||
            pkt.duration > dltaList->first().framenum))
    {
        if (dltaList->first().newPTS >= 0)
            ptsinc((uint64_t *)&origPTS, 300 * dltaList->first().newPTS);
        else
            ptsdec((uint64_t *)&origPTS, -300 * dltaList->first().newPTS);
        delta += dltaList->first().newPTS;
        dltaList->pop_front();
        VERBOSE(MPF_PROCESS, QString("Moving PTS offset of stream %1 by %2")
                             .arg(idx).arg(PtsTime(delta)));
    }
    return (delta);
}

MPEG2fixup::MPEG2fixup(const QString &inf, const QString &outf,
                       frm_dir_map_t *deleteMap,
                       const char *fmt, int norp, int fixPTS, int maxf,
                       bool showprog, int otype, void (*update_func)(float),
                       int (*check_func)())
{
    displayFrame = new Q3PtrListIterator<MPEG2frame> (vFrame);

    infile = inf;
    rx.outfile = outf;
    rx.done = 0;
    format = fmt;
    no_repeat = norp;
    fix_PTS = fixPTS;
    maxframes = maxf;
    rx.otype = otype;

    real_file_end = file_end = false;

    use_secondary = false;
    framenum = 0;
    discard = 0;
    if (deleteMap && deleteMap->count())
    {
        delMap = *deleteMap;
        if(delMap.contains(0))
        {
            discard = 1;
            delMap.remove(0);
        }
        if (delMap.begin().value() == MARK_CUT_END)
            discard = 1;
        use_secondary = true;
    }

    ext_count = 0;
    vid_id = -1;
    mpeg2_malloc_hooks(my_malloc, NULL);
    header_decoder = mpeg2_init();
    img_decoder = mpeg2_init();

    av_register_all();
    av_log_set_callback(my_av_print);

    pthread_mutex_init(&rx.mutex, NULL);
    pthread_cond_init(&rx.cond, NULL);

    //await multiplexer initialization (prevent a deadlock race)
    pthread_mutex_lock(&rx.mutex);
    pthread_create(&thread, NULL, ReplexStart, this);
    pthread_cond_wait(&rx.cond, &rx.mutex);
    pthread_mutex_unlock(&rx.mutex);

    //initialize progress stats
    showprogress = showprog;
    update_status = update_func;
    check_abort = check_func;
    if (showprogress || update_status)
    {
        if (update_status)
        {
            status_update_time = 20;
            update_status(0);
        }
        else
            status_update_time = 5;
        statustime = QDateTime::currentDateTime();
        statustime = statustime.addSecs(status_update_time);

        const QFileInfo finfo(inf);
        filesize = finfo.size();
    }
}

MPEG2fixup::~MPEG2fixup()
{
    mpeg2_close(header_decoder);
    mpeg2_close(img_decoder);


    if (inputFC)
        av_close_input_file(inputFC);

    MPEG2frame *tmpFrame;
    int max_frames = 0;

    max_frames += vFrame.count();
    while (vFrame.count())
    {
        tmpFrame = vFrame.first();
        vFrame.remove();
        delete tmpFrame;
    }
    while (vSecondary.count())
    {
        tmpFrame = vSecondary.first();
        vSecondary.remove();
        delete tmpFrame;
    }
    for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
            it != aFrame.end(); it++)
    {
        Q3PtrList<MPEG2frame> *af = &(*it);
        max_frames += af->count();
        while (af->count())
        {
            tmpFrame = af->first();
            af->remove();
            delete tmpFrame;
        }
    }
    max_frames += framePool.count();
    while (framePool.count())
        delete framePool.dequeue();
}

//#define MPEG2trans_DEBUG
#define MATCH_HEADER(ptr) (((ptr)[0] == 0x00) && ((ptr)[1] == 0x00) && ((ptr)[2] == 0x01))

static void SETBITS(unsigned char *ptr, long value, int num)
{
    static int sb_pos;
    static unsigned char *sb_ptr = 0;
    uint32_t sb_long, mask;
    int offset, offset_r, offset_b;

    if (ptr != 0)
    {
        sb_ptr = ptr;
        sb_pos = 0;
    }

    offset = sb_pos >> 3;
    offset_r = sb_pos & 0x07;
    offset_b = 32 - offset_r;
    mask = ~(((1 << num) - 1) << (offset_b - num));
    sb_long = ntohl(*((uint32_t *) (sb_ptr + offset)));
    value = value << (offset_b - num);
    sb_long = (sb_long & mask) + value;
    *((uint32_t *)(sb_ptr + offset)) = htonl(sb_long);
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
        int64_t diff;

        diff = pts1 - pts2;

        if (diff < 0){
                diff = MAX_PTS + diff;
        }
        return (diff % MAX_PTS);
}

int64_t MPEG2fixup::diff2x33(int64_t pts1, int64_t pts2)
{
    switch (cmp2x33(pts1, pts2)){
        case 0:
                return 0;
                break;

        case 1:
        case -2:
                return (pts1 -pts2);
                break;

        case 2:
                return (pts1 + MAX_PTS -pts2);
                break;

        case -1:
                return (pts1 - (pts2+ MAX_PTS));
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
        int ret;

        if (pts1 > pts2){
                if ((uint64_t)(pts1 - pts2) > MAX_PTS/2)
                        ret = -1;
                else 
                        ret = 1;
        } else if (pts1 == pts2) ret = 0; 
        else {
                if ((uint64_t)(pts2 - pts1) > MAX_PTS/2)
                        ret = 2;
                else  
                        ret = -2;
        }
        return ret;
}

int MPEG2fixup::FindMPEG2Header(uint8_t *buf, int size, uint8_t code)
{
    int i;

    for (i = 0; i < size; i++)
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
    MPEG2replex *rx = (MPEG2replex *)r;

    if (finish)
        return 0;

    return (rx->WaitBuffers());
}

MPEG2replex::MPEG2replex() :
    done(0),      otype(0),
    ext_count(0), mplex(0)
{
    memset(&vrbuf, 0, sizeof(vrbuf));
    memset(extrbuf, 0, sizeof(extrbuf));
    memset(&index_vrbuf, 0, sizeof(index_vrbuf));
    memset(index_extrbuf, 0, sizeof(index_extrbuf));
    memset(exttype, 0, sizeof(exttype));
    memset(exttypcnt, 0, sizeof(exttypcnt));
    memset(extframe, 0, sizeof(extframe));
    memset(&seq_head, 0, sizeof(seq_head)); 
}

MPEG2replex::~MPEG2replex()
{
    if (vrbuf.size)
        ring_destroy(&vrbuf);
    if (index_vrbuf.size)
        ring_destroy(&index_vrbuf);
    
    for (int i = 0; i < ext_count; i++)
    {
        if (extrbuf[i].size)
            ring_destroy(&extrbuf[i]);
        if (index_extrbuf[i].size)
            ring_destroy(&index_extrbuf[i]);
    }
}

int MPEG2replex::WaitBuffers()
{
    pthread_mutex_lock( &mutex );
    while (1)
    {
        int i, ok = 1;

        if (ring_avail(&index_vrbuf) < sizeof(index_unit))
            ok = 0;

        for (i = 0; i < ext_count; i++)
            if (ring_avail(&index_extrbuf[i]) < sizeof(index_unit))
                ok = 0;

        if (ok || done)
            break;

        pthread_cond_signal(&cond);
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    if (done)
    {
        finish_mpg(mplex);
        pthread_exit(NULL);
    }

    return 0;
}

void *MPEG2fixup::ReplexStart(void *data)
{
    MPEG2fixup *m2f = (MPEG2fixup *) data;
    m2f->rx.Start();
    return NULL;
}

void MPEG2replex::Start()
{
    int start = 1;
    multiplex_t mx;

    //array defines number of allowed audio streams
    // note that although only 1 stream is currently supported, multiplex.c
    // expects the size to by N_AUDIO
    int ext_ok[N_AUDIO];
    int video_ok = 0;

    //seq_head should be set only for the 1st sequence header.  If a new
    // seq header comes which is different, we are screwed.


    int video_delay = 0, audio_delay = 0;
    int fd_out;

    memset(&mx, 0, sizeof(mx));
    memset(ext_ok, 0, sizeof(ext_ok));

    mx.priv = (void *)this;

    fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);

    //await buffer fill
    pthread_mutex_lock(&mutex);
    pthread_cond_signal(&cond);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    mplex = &mx;

    init_multiplex(&mx, &seq_head, extframe, exttype, exttypcnt,
                   video_delay, audio_delay, fd_out, fill_buffers,
                   &vrbuf, &index_vrbuf, extrbuf, index_extrbuf, otype);
    setup_multiplex(&mx);

    while (1)
    {
        check_times( &mx, &video_ok, ext_ok, &start);
        write_out_packs( &mx, video_ok, ext_ok);
    }
}

#define INDEX_BUF (sizeof(index_unit) * 200)
void MPEG2fixup::InitReplex()
{
    // index_vrbuf contains index_units which describe a video frame
    //   it also contains the start pos of the next frame
    // index_arbuf only uses, pts, framesize, length, start, (active, err)

    //this should support > 100 frames
    uint32_t memsize = vFrame.first()->mpeg2_seq.width *
                       vFrame.first()->mpeg2_seq.height * 10;
    ring_init(&rx.vrbuf, memsize);
    ring_init(&rx.index_vrbuf, INDEX_BUF);

    memset(rx.exttype, 0, sizeof(rx.exttype));
    memset(rx.exttypcnt, 0, sizeof(rx.exttypcnt));
    int mp2_count = 0, ac3_count = 0;
    for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
            it != aFrame.end(); it++)
    {
        int i = aud_map[it.key()];
        char *lang = inputFC->streams[it.key()]->language;
        ring_init(&rx.extrbuf[i], memsize / 5);
        ring_init(&rx.index_extrbuf[i], INDEX_BUF);
        rx.extframe[i].set = 1;
        rx.extframe[i].bit_rate = getCodecContext(it.key())->bit_rate;
        rx.extframe[i].framesize = (*it).first()->pkt.size;
        strncpy(rx.extframe[i].language, lang, 4);
        switch(GetStreamType(it.key()))
        {
            case CODEC_ID_MP2:
            case CODEC_ID_MP3:
                rx.exttype[i] = 2;
                rx.exttypcnt[i] = mp2_count++;
                break;
            case CODEC_ID_AC3:
                rx.exttype[i] = 1;
                rx.exttypcnt[i] = ac3_count++;
                break;
        }
    }

    //bit_rate/400
    rx.seq_head.bit_rate = vFrame.first()->mpeg2_seq.byte_rate / 50;
    rx.seq_head.frame_rate = (vFrame.first()->mpeg2_seq.frame_period +
                         26999999ULL) / vFrame.first()->mpeg2_seq.frame_period;

    rx.ext_count = ext_count;
}

void MPEG2fixup::FrameInfo(MPEG2frame *f)
{
    QString msg = QString("Id:%1 %2 V:%3").arg(f->pkt.stream_index)
                    .arg(PtsTime(f->pkt.pts))
                    .arg(ring_free(&rx.index_vrbuf) / sizeof(index_unit));

    if (ext_count)
    {
        msg += " EXT:";
        for (int i = 0; i < ext_count; i++)
            msg += QString(" %2")
                   .arg(ring_free(&rx.index_extrbuf[i]) / sizeof(index_unit));
    }
    VERBOSE(MPF_RPLXQUEUE, msg);
}

int MPEG2fixup::AddFrame(MPEG2frame *f)
{
    index_unit iu;
    ringbuffer *rb = 0, *rbi = 0;
    int id = f->pkt.stream_index;

    memset(&iu, 0, sizeof(index_unit));
    iu.frame_start = 1;

    if (id == vid_id)
    {
        rb = &rx.vrbuf;
        rbi = &rx.index_vrbuf;
        iu.frame = GetFrameTypeN(f);
        iu.seq_header = f->isSequence;
        iu.gop = f->isGop;

        iu.gop_off = f->gopPos - f->pkt.data;
        iu.frame_off = f->framePos - f->pkt.data;
        iu.dts = f->pkt.dts * 300;
    }
    else if (GetStreamType(id) == CODEC_ID_MP2 ||
             GetStreamType(id) == CODEC_ID_MP3 ||
             GetStreamType(id) == CODEC_ID_AC3)
    {
        rb = &rx.extrbuf[aud_map[id]];
        rbi = &rx.index_extrbuf[aud_map[id]];
        iu.framesize = f->pkt.size;
    }

    if (! rb || ! rbi)
    {
        VERBOSE(MPF_IMPORTANT, "Ringbuffer pointers empty. No stream found\n");
        return 1;
    }

    iu.active = 1;
    iu.length = f->pkt.size;
    iu.pts = f->pkt.pts * 300;
    pthread_mutex_lock( &rx.mutex );

    FrameInfo(f);
    while (ring_free(rb) < (unsigned int)f->pkt.size ||
            ring_free(rbi) < sizeof(index_unit))
    {
        int i, ok = 1;

        if (rbi != &rx.index_vrbuf &&
                ring_avail(&rx.index_vrbuf) < sizeof(index_unit))
            ok = 0;

        for (i = 0; i < ext_count; i++)
            if (rbi != &rx.index_extrbuf[i] &&
                    ring_avail(&rx.index_extrbuf[i]) < sizeof(index_unit))
                ok = 0;

        if (! ok && ring_free(rb) < (unsigned int)f->pkt.size &&
                    ring_free(rbi) >= sizeof(index_unit))
        {
            // increase memory to avoid deadlock
            unsigned int inc_size = 10*(unsigned int)f->pkt.size;
            VERBOSE(MPF_IMPORTANT, QString("Increasing ringbuffer size by %1 "
                                   "to avoid deadlock").arg(inc_size));
            if (! ring_reinit(rb, rb->size + inc_size))
                ok = 1;
        }
        if (! ok)
        {
            pthread_mutex_unlock( &rx.mutex );
            //deadlock
            VERBOSE(MPF_IMPORTANT,
                    "Deadlock detected.  One buffer is full when\n"
                    "\t\tthe other is empty!  Aborting\n");
            return 1;
        }

        pthread_cond_signal(&rx.cond);
        pthread_cond_wait(&rx.cond, &rx.mutex);

        FrameInfo(f);
    }

    if (ring_write(rb, f->pkt.data, f->pkt.size)<0){
        pthread_mutex_unlock( &rx.mutex );
        VERBOSE(MPF_IMPORTANT,
                QString("Ring buffer overflow %1\n").arg(rb->size));
        return 1;
    }
    if (ring_write(rbi, (uint8_t *)&iu, sizeof(index_unit))<0){
        pthread_mutex_unlock( &rx.mutex );
        VERBOSE(MPF_IMPORTANT,
                QString("Ring buffer overflow %1\n").arg(rbi->size));
        return 1;
    }
    pthread_mutex_unlock(&rx.mutex);
    last_written_pos = f->pkt.pos;
    return 0;
}

int MPEG2fixup::InitAV(const char *inputfile, const char *type, int64_t offset)
{
    int ret;

    AVInputFormat *fmt = NULL;

    if (type)
        fmt = av_find_input_format(type);

    // Open recording
    VERBOSE(MPF_GENERAL, QString("Opening %1").arg(inputfile));

    inputFC = NULL;

    ret = av_open_input_file(&inputFC, inputfile, fmt, 0, NULL);

    if (ret != 0)
    {
        VERBOSE(MPF_IMPORTANT,
                QString("Couldn't open input file, error #%1").arg(ret));
        return 0;
    }

    mkvfile = !strcmp(inputFC->iformat->name, "mkv") ? 1 : 0;

    if (offset)
        av_seek_frame(inputFC, vid_id, offset, AVSEEK_FLAG_BYTE);

    // Getting stream information
    ret = av_find_stream_info(inputFC);

    if (ret < 0)
    {
        VERBOSE(MPF_IMPORTANT,
                QString("Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return 0;
    }

    // Dump stream information
    if (SHOW_MSG(MPF_GENERAL))
        dump_format(inputFC, 0, inputfile, 0);

    for (unsigned int i = 0; i < inputFC->nb_streams; i++)
    {
        switch (inputFC->streams[i]->codec->codec_type)
        {

            case CODEC_TYPE_VIDEO:
                if(vid_id == -1)
                    vid_id = i;
                break;

            case CODEC_TYPE_AUDIO:
                if (inputFC->streams[i]->codec->channels == 0)
                {
                    VERBOSE(MPF_GENERAL, QString(
                            "Skipping invalid audio stream: %1").arg(i));
                    break;
                }
                if (inputFC->streams[i]->codec->codec_id == CODEC_ID_AC3 ||
                    inputFC->streams[i]->codec->codec_id == CODEC_ID_MP3 ||
                    inputFC->streams[i]->codec->codec_id == CODEC_ID_MP2)
                {
                    aud_map[i] = ext_count++;
                    aFrame[i] = Q3PtrList<MPEG2frame> ();
                }
                else
                    VERBOSE(MPF_GENERAL, QString(
                            "Skipping unsupported audio stream: %1")
                            .arg(inputFC->streams[i]->codec->codec_id));
                break;

            default:
                VERBOSE(MPF_GENERAL, QString(
                        "Skipping unsupported codec %1 on stream %2")
                        .arg(inputFC->streams[i]->codec->codec_type).arg(i));
        }
    }

    return 1;
}

void MPEG2fixup::SetFrameNum(uint8_t *ptr, int num)
{
    SETBITS(ptr + 4, num, 10);
}

void MPEG2fixup::AddSequence(MPEG2frame *frame1, MPEG2frame *frame2)
{
    if (frame1->isSequence || !frame2->isSequence)
        return ;

    int head_size = (frame2->framePos - frame2->pkt.data);

    frame1->ensure_size(frame1->pkt.size + head_size);
    memmove(frame1->pkt.data + head_size, frame1->pkt.data, frame1->pkt.size);
    memcpy(frame1->pkt.data, frame2->pkt.data, head_size);
    frame1->pkt.size+=head_size;
    ProcessVideo(frame1, header_decoder);
    if (SHOW_MSG(MPF_PROCESS))
    {
        static int count = 0;
        QString filename = QString("hdr%1.yuv").arg(count++);
        QByteArray fname = filename.toAscii();
        WriteFrame(fname.constData(), &frame1->pkt);
    }
}

bool MPEG2fixup::ProcessVideo(MPEG2frame *vf, mpeg2dec_t *dec)
{
    int state = -1;
    int last_pos = 0;
    mpeg2_info_t *info;

    if (dec == header_decoder)
    {
        mpeg2_reset(dec, 0);
        vf->isSequence = 0;
        vf->isGop = 0;
    }

    info = (mpeg2_info_t *)mpeg2_info(dec);

    mpeg2_buffer(dec, vf->pkt.data, vf->pkt.data + vf->pkt.size);

    while(state != STATE_PICTURE)
    {
        state = mpeg2_parse(dec);

        if (dec == header_decoder)
        {
            switch (state)
            {

                case STATE_SEQUENCE:
                case STATE_SEQUENCE_MODIFIED:
                case STATE_SEQUENCE_REPEATED:
                    memcpy(&vf->mpeg2_seq, info->sequence,
                           sizeof(mpeg2_sequence_t));
                    vf->isSequence = 1;
                    break;

                case STATE_GOP:
                    memcpy(&vf->mpeg2_gop, info->gop, sizeof(mpeg2_gop_t));
                    vf->isGop = 1;
                    vf->gopPos = vf->pkt.data + last_pos;
                    //pd->adjustFrameCount=0;
                    break;

                case STATE_PICTURE:
                    memcpy(&vf->mpeg2_pic, info->current_picture,
                           sizeof(mpeg2_picture_t));
                    vf->framePos = vf->pkt.data + last_pos;
                    break;

                case STATE_BUFFER:
                    VERBOSE(MPF_GENERAL, "Warning: partial frame found!");
                    return 1;
            }
        }
        else if (state == STATE_BUFFER)
        {
            WriteData("abort.dat", vf->pkt.data, vf->pkt.size);
            VERBOSE(MPF_IMPORTANT, QString(
                    "Failed to decode frame.  Position was: %1").arg(last_pos));
            return -1;
        } 
        last_pos = (vf->pkt.size - mpeg2_getpos(dec)) - 4;
    }

    if (dec != header_decoder)
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
            uint8_t tmp[8] = {0x00, 0x00, 0x01, 0xb2, 0xff, 0xff, 0xff, 0xff};
            mpeg2_buffer(dec, tmp, tmp + 8);
            mpeg2_parse(dec);
        }   
    }

    if (SHOW_MSG(MPF_DECODE))
    {
        QString msg = QString("");
        //msg += QString("unused:%1 ")
        //               .arg(vf->pkt.size - mpeg2_getpos(dec));

        if (vf->isSequence)
            msg += QString("%1x%2 P:%3 ").arg(info->sequence->width)
                .arg(info->sequence->height).arg(info->sequence->frame_period);

        if (info->gop)
        {
            QString gop;
            gop.sprintf("%02d:%02d:%02d:%03d ",
                        info->gop->hours, info->gop->minutes,
                        info->gop->seconds, info->gop->pictures);
            msg += gop;
        }
        if (info->current_picture) {
            int ct = info->current_picture->flags & PIC_MASK_CODING_TYPE;
            char coding_type = (ct == PIC_FLAG_CODING_TYPE_I) ? 'I' :
                               ((ct == PIC_FLAG_CODING_TYPE_P) ? 'P' :
                                ((ct == PIC_FLAG_CODING_TYPE_B) ? 'B' :
                                 ((ct == PIC_FLAG_CODING_TYPE_D) ?'D' : 'X')));
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
        msg += QString("pos: %1").arg(vf->pkt.pos);
        VERBOSE(MPF_DECODE, msg);
    }

    return 0;
}

void MPEG2fixup::WriteFrame(const char *filename, MPEG2frame *f)
{
    MPEG2frame *tmpFrame = GetPoolFrame(f);
    if (tmpFrame == NULL)
        return;
    if (! tmpFrame->isSequence)
    {
        Q3PtrListIterator<MPEG2frame> it (vFrame);
        while (*it)
        {
            if((*it)->isSequence)
            {
                AddSequence(tmpFrame, *it);
                break;
            }
            ++it;
        }
    }
    WriteFrame(filename, &tmpFrame->pkt);
    framePool.enqueue(tmpFrame);
}
   
void MPEG2fixup::WriteFrame(const char *filename, AVPacket *pkt)
{

    MPEG2frame *tmpFrame = GetPoolFrame(pkt);
    if (tmpFrame == NULL)
        return;

    QString fname = QString(filename) + ".enc";
    QByteArray aname = fname.local8Bit();
    WriteData(aname, pkt->data, pkt->size);

    mpeg2dec_t *tmp_decoder = mpeg2_init();
    mpeg2_info_t *info = (mpeg2_info_t *)mpeg2_info(tmp_decoder);

    while (!info->display_picture)
    {
        if (ProcessVideo(tmpFrame, tmp_decoder))
        {
            delete tmpFrame;
            return;
        }
    }

    WriteYUV(filename, info);
    framePool.enqueue(tmpFrame);
    mpeg2_close(tmp_decoder);
}

void MPEG2fixup::WriteYUV(const char *filename, const mpeg2_info_t *info)
{
    int fh = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (fh == -1) {
        VERBOSE(MPF_IMPORTANT, QString("Couldn't open file %1").arg(filename));
        return;
    }

    int ret = write(fh, info->display_fbuf->buf[0],
                    info->sequence->width * info->sequence->height);
    if (ret < 0)
    {
        VERBOSE(MPF_IMPORTANT, QString("write failed %1. %2").arg(filename)
                .arg(strerror(errno)));
        goto closefd;
    }
    ret = write(fh, info->display_fbuf->buf[1],
                info->sequence->chroma_width * info->sequence->chroma_height);
    if (ret < 0)
    {
        VERBOSE(MPF_IMPORTANT, QString("write failed %1. %2").arg(filename)
                .arg(strerror(errno)));
        goto closefd;
    }
    ret = write(fh, info->display_fbuf->buf[2],
                info->sequence->chroma_width * info->sequence->chroma_height);
    if (ret < 0)
    {
        VERBOSE(MPF_IMPORTANT, QString("write failed %1. %2").arg(filename)
                .arg(strerror(errno)));
        goto closefd;
    }
closefd:
    close(fh);
}

void MPEG2fixup::WriteData(const char *filename, uint8_t *data, int size)
{
    int fh = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    if (fh == -1) {
        VERBOSE(MPF_IMPORTANT, QString("Couldn't open file %1").arg(filename));
        return;
    }

    int ret = write(fh, data, size);
    if (ret < 0)
        VERBOSE(MPF_IMPORTANT, QString("write failed %1. %2").arg(filename)
                .arg(strerror(errno)));
    close(fh);
}

extern "C" {
#include "helper.h"
}

int MPEG2fixup::BuildFrame(AVPacket *pkt, QString fname)
{
    const mpeg2_info_t *info;
    int outbuf_size;
    uint16_t intra_matrix[64] ATTR_ALIGN(16);
    AVFrame *picture;
    AVCodecContext *c = NULL;
    AVCodec *out_codec;

    info = mpeg2_info(img_decoder);
    if (! info->display_fbuf)
        return 1;

    outbuf_size = info->sequence->width * info->sequence->height * 2;

    if (!fname.isEmpty())
    {
        QString tmpstr = fname + ".yuv";
        QByteArray tmp = tmpstr.toAscii(); 
        WriteYUV(tmp.constData(), info);
    }

    picture = avcodec_alloc_frame();

    pkt->data = (uint8_t *)av_malloc(outbuf_size);

    picture->data[0] = info->display_fbuf->buf[0];
    picture->data[1] = info->display_fbuf->buf[1];
    picture->data[2] = info->display_fbuf->buf[2];

    picture->linesize[0] = info->sequence->width;
    picture->linesize[1] = info->sequence->chroma_width;
    picture->linesize[2] = info->sequence->chroma_width;

    picture->opaque = info->display_fbuf->id;

    copy_quant_matrix(img_decoder, intra_matrix);

    if (info->display_picture->nb_fields % 2)
        picture->top_field_first = !(info->display_picture->flags &
                                     PIC_FLAG_TOP_FIELD_FIRST);
    else
        picture->top_field_first = !!(info->display_picture->flags &
                                      PIC_FLAG_TOP_FIELD_FIRST);

    picture->interlaced_frame = !(info->display_picture->flags &
                                  PIC_FLAG_PROGRESSIVE_FRAME);

    out_codec = avcodec_find_encoder(CODEC_ID_MPEG2VIDEO);

    if (! out_codec)
    {
        free(picture);
        VERBOSE(MPF_IMPORTANT, "Couldn't find MPEG2 encoder");
        return 1;
    }

    c = avcodec_alloc_context();

    //We disable all optimizations for the time being.  There shouldn't be too
    //much encoding going on, and the optimizations have been shown to cause
    //corruption in some cases
    c->dsp_mask = 0xffff;

    //NOTE: The following may seem wrong, but avcodec requires
    //sequence->progressive == frame->progressive
    //We fix the discrepancy by discarding avcodec's sequence header, and
    //replace it with the original
    if(picture->interlaced_frame)
        c->flags |= CODEC_FLAG_INTERLACED_DCT;

    c->bit_rate = info->sequence->byte_rate << 3; //not used
    c->bit_rate_tolerance = c->bit_rate >> 2; //not used
    c->width = info->sequence->width;
    c->height = info->sequence->height;
    av_reduce(&c->time_base.num, &c->time_base.den,
              info->sequence->frame_period, 27000000LL, 100000);
    c->pix_fmt = PIX_FMT_YUV420P;
    c->max_b_frames = 0;
    c->has_b_frames = 0;
    c->rc_buffer_aggressivity = 1;
    //  c->profile = vidCC->profile;
    //  c->level = vidCC->level;
    c->rc_buffer_size = 0;
    c->gop_size = 0; // this should force all i-frames
    //  c->flags=CODEC_FLAG_LOW_DELAY;

    if (intra_matrix[0] == 0x08)
        c->intra_matrix = intra_matrix;

    c->qmin = c->qmax = 2;

    picture->pts = AV_NOPTS_VALUE;
    picture->key_frame = 1;
    picture->pict_type = 0;
    picture->type = 0;
    picture->quality = 0;

    if (avcodec_open(c, out_codec) < 0)
    {
        free(picture);
        VERBOSE(MPF_IMPORTANT, "could not open codec");
        return 1;
    }

    pkt->size = avcodec_encode_video(c, pkt->data, outbuf_size, picture);

    if (pkt->size <= 0)
    {
        free(picture);
        VERBOSE(MPF_IMPORTANT, QString("avcodec_encode_video failed (%1)").arg(pkt->size));
        return 1;
    }

    if (!fname.isEmpty())
    {
        QString ename = fname + ".enc";
        QByteArray aename = ename.toLocal8Bit();
        WriteData(aename.constData(), pkt->data, pkt->size);

        QString yname = fname + ".enc.yuv";
        QByteArray ayname = yname.toLocal8Bit();
        WriteFrame(ayname.constData(), pkt);
    }
    int delta = FindMPEG2Header(pkt->data, pkt->size, 0x00);
    //  out_size=avcodec_encode_video(c, outbuf, outbuf_size, picture);
    pkt->size -= delta; // a hack to get to the picture frame
    memmove(pkt->data, pkt->data + delta, pkt->size);
    SetRepeat(pkt->data, pkt->size, info->display_picture->nb_fields,
               !!(info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST));

    avcodec_close(c);
    av_freep(&c);
    av_freep(&picture);

    return 0;
}

#define MAX_FRAMES 2000
MPEG2frame *MPEG2fixup::GetPoolFrame(AVPacket *pkt)
{
    MPEG2frame *f;
    static int frame_count = 0;

    if (framePool.isEmpty())
    {
        if (frame_count >= MAX_FRAMES)
        {
            VERBOSE(MPF_IMPORTANT, "No more queue slots!");
            return NULL;
        }
        f = new MPEG2frame(pkt->size);
        frame_count++;
    }
    else
        f = framePool.dequeue();

    f->set_pkt(pkt);

    return f;
}

MPEG2frame *MPEG2fixup::GetPoolFrame(MPEG2frame *f)
{
    MPEG2frame *tmpFrame = GetPoolFrame(&f->pkt);
    if (!tmpFrame)
        return tmpFrame;

    tmpFrame->isSequence = f->isSequence;
    tmpFrame->isGop      = f->isGop;
    tmpFrame->mpeg2_seq  = f->mpeg2_seq;
    tmpFrame->mpeg2_gop  = f->mpeg2_gop;
    tmpFrame->mpeg2_pic  = f->mpeg2_pic;
    return tmpFrame;
}

int MPEG2fixup::GetFrame(AVPacket *pkt)
{
    int ret;

    while (1)
    {
        bool done = 0;
        if (unreadFrames.count())
        {
            vFrame.append(unreadFrames.dequeue());
            if (real_file_end && ! unreadFrames.count())
                file_end = true;
            return (file_end == true);
        }
        while (! done)
        {
            pkt->pts = AV_NOPTS_VALUE;
            pkt->dts = AV_NOPTS_VALUE;
            ret = av_read_frame(inputFC, pkt);

            if (ret < 0)
            {
                //insert a bogus frame (this won't be written out)
                if(vFrame.isEmpty())
                {
                    VERBOSE(MPF_IMPORTANT, "Found end of file without finding "
                                           " any frames");
                    return TRANSCODE_EXIT_UNKNOWN_ERROR;
                }
                MPEG2frame *tmpFrame = GetPoolFrame(&vFrame.last()->pkt);
                if (tmpFrame == NULL)
                    return TRANSCODE_EXIT_UNKNOWN_ERROR;
                vFrame.append(tmpFrame);
                real_file_end = true;
                file_end = true;
                return 1;
            }

            if (pkt->stream_index == vid_id ||
                    aFrame.contains(pkt->stream_index))
                done = 1;
            else 
                av_free_packet(pkt);
        }
        pkt->duration = framenum++;
        if ((showprogress || update_status) &&
            QDateTime::currentDateTime() > statustime)
        {
            float percent_done = 100.0 * pkt->pos / filesize;
            if (update_status)
                update_status(percent_done);
            if (showprogress)
                VERBOSE(MPF_IMPORTANT, QString("%1% complete")
                        .arg(percent_done, 0, 'f', 1));
            if (check_abort && check_abort())
                return REENCODE_STOPPED;
            statustime = QDateTime::currentDateTime();
            statustime = statustime.addSecs(status_update_time);
        }

#ifdef DEBUG_AUDIO
        VERBOSE(MPF_DECODE, QString("Stream: %1 PTS: %2 DTS: %3 pos: %4")
              .arg(pkt->stream_index)
              .arg((pkt->pts == AV_NOPTS_VALUE) ? "NONE" : PtsTime(pkt->pts))
              .arg((pkt->dts == AV_NOPTS_VALUE) ? "NONE" : PtsTime(pkt->dts))
              .arg(pkt->pos));
#endif

        MPEG2frame *tmpFrame = GetPoolFrame(pkt);
        if (tmpFrame == NULL)
            return TRANSCODE_EXIT_UNKNOWN_ERROR;
        switch (inputFC->streams[pkt->stream_index]->codec->codec_type)
        {

            case CODEC_TYPE_VIDEO:
                vFrame.append(tmpFrame);
                av_free_packet(pkt);

                if (! ProcessVideo(vFrame.last(), header_decoder))
                    return 0;
                framePool.enqueue(vFrame.last());
                vFrame.removeLast();

                break;

            case CODEC_TYPE_AUDIO:
                aFrame[pkt->stream_index].append(tmpFrame);
                av_free_packet(pkt);
                return 0;

            default:
                framePool.enqueue(tmpFrame);
                av_free_packet(pkt);
                return TRANSCODE_EXIT_UNKNOWN_ERROR;
        }
    }
}

bool MPEG2fixup::FindStart()
{
    AVPacket pkt;
    int state = 0;
    QMap <int, bool> found;

    av_init_packet(&pkt);

    while (!state)
    {
        if (GetFrame(&pkt))
            return false;
        if (vid_id == pkt.stream_index)
        {
            while (! vFrame.isEmpty())
            {
                if (vFrame.first()->isSequence)
                {
                    if (pkt.pos == vFrame.first()->pkt.pos)
                    {
                        if ((uint64_t)pkt.pts != AV_NOPTS_VALUE ||
                            (uint64_t)pkt.dts != AV_NOPTS_VALUE)
                        {
                            if ((uint64_t)pkt.pts == AV_NOPTS_VALUE)
                                vFrame.first()->pkt.pts = pkt.dts;
                            VERBOSE(MPF_PROCESS, "Found 1st valid video frame");
                            break;
                        }
                    }
                    else
                        break;
                }
                VERBOSE(MPF_PROCESS, "Dropping V packet");

                framePool.enqueue( vFrame.first());

                vFrame.removeFirst();
            }
        }

        if (vFrame.isEmpty())
            continue;

        for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
                it != aFrame.end(); it++)
        {
            Q3PtrList<MPEG2frame> *af = &(*it);

            if (found.contains(it.key()))
                continue;

            while (! af->isEmpty())
            {
                int64_t delta = diff2x33(af->first()->pkt.pts,
                                         vFrame.first()->pkt.pts);
                if (delta < -180000 || delta > 180000) //2 seconds
                {
                    //Check all video sequence packets against current
                    //audio packet
                    MPEG2frame *found = NULL;
                    while (vFrame.current())
                    {
                        if(vFrame.current()->isSequence)
                        {
                            int64_t dlta1 = diff2x33(af->first()->pkt.pts,
                                                     vFrame.current()->pkt.pts);
                            if (dlta1 >= -180000 && dlta1 <= 180000)
                            {
                                found = vFrame.current();
                                delta = dlta1;
                                break;
                            }
                        }
                        vFrame.next();
                    }
                    if (found)
                    {
                        while (vFrame.first() != found)
                        {
                            framePool.enqueue( vFrame.first());
                            vFrame.removeFirst();
                        }
                    }
                }
                if (delta < -180000 || delta > 180000) //2 seconds
                {
                        VERBOSE(MPF_PROCESS,
                                QString("Dropping A packet from stream %1")
                                       .arg(it.key()));
                        VERBOSE(MPF_PROCESS,
                                QString("     A:%1 V:%2")
                                        .arg(PtsTime(af->first()->pkt.pts))
                                        .arg(PtsTime(vFrame.first()->pkt.pts)));
                        framePool.enqueue( af->first());
                        af->removeFirst();
                        continue;
                }
                if (delta < 0 && af->count() > 1)
                {
                    if (cmp2x33(af->next()->pkt.pts,
                                vFrame.first()->pkt.pts) > 0)
                    {
                        VERBOSE(MPF_PROCESS, QString("Found useful audio "
                                "frame from stream %1").arg(it.key()));
                        found[it.key()] = 1;
                        break;
                    }
                    else
                    {
                        VERBOSE(MPF_PROCESS,
                                QString("Dropping A packet from stream %1")
                                       .arg(it.key()));
                        framePool.enqueue( af->first());
                        af->removeFirst();
                        continue;
                    }
                }
                else if (delta >= 0)
                {
                    VERBOSE(MPF_PROCESS, QString("Found useful audio "
                            "frame from stream %1").arg(it.key()));
                    found[it.key()] = 1;
                    break;
                }

                if (af->count() == 1)
                    break;
            }
        }

        state = (found.count() == aFrame.count());
    }

    return true;
}

void MPEG2fixup::SetRepeat(MPEG2frame *vf, int fields, bool topff)
{
    vf->mpeg2_pic.nb_fields = 2;
    SetRepeat(vf->framePos, vf->pkt.data + vf->pkt.size - vf->framePos,
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
            return ;
        }

        ptr++;
    }
}

MPEG2frame *MPEG2fixup::FindFrameNum(int frameNum)
{
    Q3PtrListIterator<MPEG2frame> it (vFrame);

    while (*it)
    {
        if (GetFrameNum(it.current()) == frameNum)
            return it.current();

        ++it;
    }

    return NULL;
}

void MPEG2fixup::RenumberFrames(int start_pos, int delta)
{
    // FIXME: We sometimes end up here with invalid frames pointers in the list
    //        which needs fixing - symptom is segfaults of mythtranscode when
    //        dereferencing the iterator value
    Q3PtrListIterator<MPEG2frame> it (vFrame);
    
    it+= start_pos;

    while (!it.atLast() || (it.atFirst() && (*it)->isSequence))
    {
        SetFrameNum((*it)->framePos,
                    GetFrameNum((*it)) + delta);
        (*it)->mpeg2_pic.temporal_reference += delta;

        ++it;
    }
}

void MPEG2fixup::StoreSecondary()
{
    while (vSecondary.first())
    {
        framePool.enqueue(vSecondary.current());
        vSecondary.remove();
    }
    while (vFrame.first() != vFrame.getLast())
    {
        if (use_secondary && GetFrameTypeT(vFrame.current()) != 'B')
            vSecondary.append(vFrame.current());
        else
            framePool.enqueue(vFrame.current());
        vFrame.remove();
    }
}

int MPEG2fixup::PlaybackSecondary()
{
    int frame_num = 0;
    mpeg2_reset(img_decoder, 1);
    for (vSecondary.first(); vSecondary.current(); vSecondary.next())
    {
        
        SetFrameNum(vSecondary.current()->framePos, frame_num++);
        if (ProcessVideo(vSecondary.current(), img_decoder) < 0)
            return 1;
    }
    return 0;
}

MPEG2frame *MPEG2fixup::DecodeToFrame(int frameNum, int skip_reset)
{
    MPEG2frame *spare = NULL;
    int found = 0;
    bool skip_first = false;
    const mpeg2_info_t * info = mpeg2_info(img_decoder);

    if (displayFrame->current()->isSequence)
    {
        skip_first = true;
        if (! skip_reset &&
                   (! displayFrame->atLast() || displayFrame->atFirst()))
            mpeg2_reset(img_decoder, 1);
    }


    spare = FindFrameNum(frameNum);
    if (! spare)
        return NULL;

    int framePos = vFrame.findRef(spare);

    int curPos = vFrame.findRef(displayFrame->current());
    while (! displayFrame->atLast())
    {
        if (ProcessVideo(displayFrame->current(), img_decoder) < 0)
            return NULL;

        if (! skip_first && curPos >= framePos && info->display_picture &&
              (int)info->display_picture->temporal_reference >= frameNum)
        {
            found = 1;
            ++(*displayFrame);
            break;
        }

        skip_first = false;
        ++curPos;
        ++(*displayFrame);
    }

    if (!found)
    {
        int tmpFrameNum = frameNum;
        MPEG2frame *tmpFrame = GetPoolFrame(&spare->pkt);
        if (tmpFrame == NULL)
            return NULL;
        tmpFrame->framePos = tmpFrame->pkt.data +
                             (spare->framePos - spare->pkt.data);

        while (! info->display_picture ||
                (int)info->display_picture->temporal_reference < frameNum)
        {
            SetFrameNum(tmpFrame->framePos, ++tmpFrameNum);
            if (ProcessVideo(tmpFrame, img_decoder) < 0)
                return NULL;
        }

        framePool.enqueue(tmpFrame);
    }

    if ((int)info->display_picture->temporal_reference > frameNum)
    {
        // the frame in question doesn't exist.  We have no idea where we are.
        // reset the displayFrame so we start searching from the beginning next
        // time
        displayFrame->toFirst();
        VERBOSE(MPF_IMPORTANT, QString("Frame %1 > %2."
                " Corruption likely at pos: %3")
               .arg(info->display_picture->temporal_reference)
               .arg(frameNum).arg(spare->pkt.pos));
    }
    return spare;
}

int MPEG2fixup::ConvertToI(Q3PtrList<MPEG2frame> *orderedFrames, int headPos)
{
    MPEG2frame *spare = NULL;
    AVPacket pkt;
    static int ins_count = 0;

    //head_pos == 0 means that we are decoding B frames after a seq_header
    if (headPos == 0)
        if (PlaybackSecondary())
            return 1;

    Q3PtrListIterator<MPEG2frame> it(*orderedFrames);
    for (it.toFirst(); *it; ++it)
    {
        int i = GetFrameNum(it.current());
        if ((spare = DecodeToFrame(i, headPos == 0)) == NULL)
            return 1;
        if(GetFrameTypeT(spare) == 'I')
            continue;
        pkt = spare->pkt;
        //pkt.data is a newly malloced area
        {
            QString fname = (SHOW_MSG(MPF_PROCESS)) ?
                   QString("cnv%1").arg(ins_count++) : QString();
            if(BuildFrame(&pkt, fname))
                return 1;
            VERBOSE(MPF_GENERAL,
                    QString("Converting frame #%1 from %2 to I %3")
                           .arg(i).arg(GetFrameTypeT(spare)).arg(fname));
        }
        spare->set_pkt(&pkt);
        av_free(pkt.data);
        SetFrameNum(spare->pkt.data, GetFrameNum(spare));
        ProcessVideo(spare, header_decoder); //process this new frame
    }

    //reorder frames
    spare = vFrame.at(headPos);

    vFrame.remove();

    vFrame.insert(headPos + orderedFrames->count() - 1, spare);

    vFrame.at(headPos);
    return 0;
}

int MPEG2fixup::InsertFrame(int frameNum, int64_t deltaPTS,
                             int64_t ptsIncrement, int64_t initPTS)
{
    MPEG2frame *spare = NULL;
    AVPacket pkt;
    int increment = 0;
    static int ins_count = 0;

    if ((spare = DecodeToFrame(frameNum, 0)) == NULL)
        return -1;
    pkt = spare->pkt;
    //pkt.data is a newly malloced area
    {
        QString fname = (SHOW_MSG(MPF_PROCESS) ?
                            (QString("ins%1").arg(ins_count++)) : QString());
        if (BuildFrame(&pkt, fname))
            return -1;
        VERBOSE(MPF_GENERAL, QString("Inserting %1 I-Frames after #%2 %3")
                                    .arg((int)(deltaPTS / ptsIncrement))
                                    .arg(GetFrameNum(spare)).arg(fname));
    }
    inc2x33(&pkt.pts, ptsIncrement * GetNbFields(spare) / 2 + initPTS);

    vFrame.findRef(spare);

    while (vFrame.next() != vFrame.getLast() &&
            GetFrameTypeT(vFrame.current()) == 'B')
        spare = vFrame.current();

    vFrame.findRef(spare);

    while (deltaPTS > 0)
    {
        MPEG2frame *tmpFrame;
        increment++;
        pkt.dts = pkt.pts;
        SetFrameNum(pkt.data, ++frameNum);
        tmpFrame = GetPoolFrame(&pkt);
        if (tmpFrame == NULL)
            return -1;
        vFrame.insert(vFrame.at() + 1, tmpFrame);
        ProcessVideo(vFrame.current(), header_decoder); //process new frame

        inc2x33(&pkt.pts, ptsIncrement);
        deltaPTS -= ptsIncrement;
    }

    av_free(pkt.data);

    // update frame # for all frames in this group
    RenumberFrames(vFrame.at() + 1, increment);

    return increment;
}

void MPEG2fixup::AddRangeList(QStringList rangelist, int type)
{
    QStringList::iterator i;
    frm_dir_map_t *mapPtr;
    if (type == MPF_TYPE_CUTLIST)
    {
        mapPtr = &delMap;
        discard = 0;
    }
    else
        mapPtr = &saveMap;

    mapPtr->clear();

    for (i = rangelist.begin(); i != rangelist.end(); ++i)
    {
        long long start = 0, end = 0;
        QByteArray tmp = (*i).toAscii();
        if (sscanf(tmp.constData(), "%lld - %lld", &start, &end) == 2)
        {
            if(start == 0)
            {
                if (type == MPF_TYPE_CUTLIST)
                    discard = 1;
            }
            else
                mapPtr->insert(start - 1, MARK_CUT_START);
            mapPtr->insert(end, MARK_CUT_END);
        }
    }
    if (rangelist.count())
        use_secondary = true;
}

void MPEG2fixup::ShowRangeMap(frm_dir_map_t *mapPtr, QString msg)
{
    if (mapPtr->count())
    {
        int64_t start = 0;
        frm_dir_map_t::iterator it = mapPtr->begin();
        for (; it != mapPtr->end(); ++it)
            if (*it == 0)
                msg += QString("\n\t\t%1 - %2").arg(start).arg(it.key());
            else
                start = it.key();
        VERBOSE(MPF_PROCESS, msg);
    }
}

Q3PtrList<MPEG2frame> MPEG2fixup::ReorderDTStoPTS(Q3PtrList<MPEG2frame> *dtsOrder)
{
    Q3PtrList<MPEG2frame> Lreorder;
    int pos = dtsOrder->at();

    if (dtsOrder->current() == dtsOrder->getLast())
        return Lreorder;
    for (dtsOrder->next();
         dtsOrder->current() != dtsOrder->getLast() &&
           GetFrameTypeT(dtsOrder->current()) == 'B';
         dtsOrder->next())
    {
        Lreorder.append(dtsOrder->current());
    }

    Lreorder.append(dtsOrder->at(pos));
    return Lreorder;
}

void MPEG2fixup::InitialPTSFixup(MPEG2frame *curFrame, int64_t &origvPTS,
                                 int64_t &PTSdiscrep, int numframes, bool fix)
{
    int64_t tmpPTS = diff2x33(curFrame->pkt.pts,
                              origvPTS / 300);

    if ((uint64_t)curFrame->pkt.pts == AV_NOPTS_VALUE)
    {
        VERBOSE(MPF_PROCESS,
            QString("Found frame %1 with missing PTS at %2")
                .arg(GetFrameNum(curFrame))
                .arg(PtsTime(origvPTS / 300)));
        if (fix)
            curFrame->pkt.pts = origvPTS / 300;
        else
            PTSdiscrep = AV_NOPTS_VALUE;
    }
    else if (tmpPTS < -ptsIncrement ||
             tmpPTS > ptsIncrement*numframes)
    {
        if (tmpPTS != PTSdiscrep)
        {
            PTSdiscrep = tmpPTS;
            VERBOSE(MPF_PROCESS,
                QString("Found invalid PTS (off by %1) at %2")
                       .arg(PtsTime(tmpPTS))
                       .arg(PtsTime(origvPTS / 300)));
        }
        if (fix)
            curFrame->pkt.pts = origvPTS / 300;
    }
    else
    {
        origvPTS = curFrame->pkt.pts * 300;
    }
    ptsinc((uint64_t *)&origvPTS,
            (uint64_t)(150 * ptsIncrement *
                       GetNbFields(curFrame)));
}

int MPEG2fixup::Start()
{
    //NOTE: expectedvPTS/DTS are in units of SCR (300*PTS) to allow for better
    //accounting of rounding errors (still won't be right, but better)
    int64_t expectedvPTS, expectedPTS[N_AUDIO];
    int64_t expectedDTS = 0, lastPTS = 0, initPTS = 0, deltaPTS = 0;
    int64_t origvPTS = 0, origaPTS[N_AUDIO];
    int64_t cutStartPTS = 0, cutEndPTS = 0;
    uint64_t frame_count = 0;
    int new_discard_state = 0;
    int ret;
    QMap<int, int> af_dlta_cnt, cutState;
    //  int i;

    AVPacket pkt, lastRealvPkt;

    QByteArray ainfile = infile.toLocal8Bit();
    if (! InitAV(ainfile.constData(), format, 0))
    {
        return (TRANSCODE_EXIT_UNKNOWN_ERROR);
    }

    if (! FindStart())
        return (TRANSCODE_EXIT_UNKNOWN_ERROR);

    av_init_packet(&pkt);

    ptsIncrement = vFrame.first()->mpeg2_seq.frame_period / 300;

    initPTS = vFrame.current()->pkt.pts;

    VERBOSE(MPF_GENERAL, QString("#%1 PTS:%2 Delta: 0.0ms queue: %3")
                         .arg(vid_id).arg(PtsTime(vFrame.current()->pkt.pts))
                         .arg(vFrame.count()));

    for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
            it != aFrame.end(); it++)
    {
        Q3PtrList<MPEG2frame> *af = &(*it);
        deltaPTS = diff2x33(vFrame.current()->pkt.pts, af->first()->pkt.pts);
        VERBOSE(MPF_GENERAL, QString("#%1 PTS:%2 Delta: %3ms queue: %4")
                             .arg(it.key())
                             .arg(PtsTime(af->current()->pkt.pts))
                             .arg(1000.0*deltaPTS / 90000.0).arg(af->count()));

        if (cmp2x33(af->current()->pkt.pts, initPTS) < 0)
            initPTS = af->current()->pkt.pts;
    }

    initPTS -= 16200; //0.18 seconds back to prevent underflow

    PTSOffsetQueue poq(vid_id, aFrame.keys(), initPTS);

    VERBOSE(MPF_PROCESS, QString("ptsIncrement: %1 Frame #: %2 PTS-adjust: %3")
                         .arg(ptsIncrement).arg(GetFrameNum(vFrame.current()))
                         .arg(PtsTime(initPTS)));


    origvPTS = 300 * udiff2x33(vFrame.first()->pkt.pts,
                     ptsIncrement * GetFrameNum(vFrame.current()));
    expectedvPTS = 300 * (udiff2x33(vFrame.first()->pkt.pts, initPTS) -
                         (ptsIncrement * GetFrameNum(vFrame.current())));
    expectedDTS = expectedvPTS - 300 * ptsIncrement;

    if (discard)
    {
        cutStartPTS = origvPTS / 300;
    }

    for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
            it != aFrame.end(); it++)
    {
        Q3PtrList<MPEG2frame> *af = &(*it);
        origaPTS[it.key()] = af->first()->pkt.pts * 300;
        expectedPTS[it.key()] = udiff2x33(af->first()->pkt.pts, initPTS);
        af_dlta_cnt[it.key()] = 0;
        cutState[it.key()] = !!(discard);
    }

    ShowRangeMap(&delMap, "Cutlist:");
    ShowRangeMap(&saveMap, "Same Range:");

    InitReplex();

    while (1)
    {
        /* read packet */
        if (! file_end)
        {
            if ((ret = GetFrame(&pkt)) < 0)
                return ret;
        }
        else
            break;

        if (vFrame.count() && (file_end || vFrame.getLast()->isSequence))
        {
            MPEG2frame *seqFrame;
            if (ptsIncrement != vFrame.first()->mpeg2_seq.frame_period / 300)
            {
                VERBOSE(MPF_IMPORTANT,
                    QString("WARNING - Unsupported FPS change from %1 to %2")
                    .arg(90000.0 / ptsIncrement, 0, 'f', 2)
                    .arg(27000000.0 / vFrame.first()->mpeg2_seq.frame_period,
                         0, 'f', 2));
            }
            displayFrame->toFirst();

            // since we might reorder the frames when coming out of a cutpoint
            // me need to save the first frame here, as it is guaranteed to
            // have a sequence header.
            seqFrame = vFrame.current();

            while (vFrame.current() != vFrame.getLast())
            {
                bool ptsorder_eq_dtsorder = false;
                int frame_pos = vFrame.at();
                int64_t dtsExtra = 0, PTSdiscrep = 0;
                Q3PtrList<MPEG2frame> Lreorder;
                MPEG2frame *markedFrame = NULL, *markedFrameP = NULL;

                if (expectedvPTS != expectedDTS + ptsIncrement * 300)
                {
                    VERBOSE(MPF_IMPORTANT, QString("expectedPTS != expectedDTS"
                              "+ptsIncrement"));
                    VERBOSE(MPF_PROCESS, QString("%1 != %2 +%3")
                                       .arg(PtsTime(expectedvPTS / 300))
                                       .arg(PtsTime(expectedDTS / 300))
                                       .arg(PtsTime(ptsIncrement)));
                    VERBOSE(MPF_PROCESS, QString("%1 != %2 +%3")
                                       .arg(expectedvPTS)
                                       .arg(expectedDTS)
                                       .arg(ptsIncrement));
                    return TRANSCODE_EXIT_UNKNOWN_ERROR;
                }
                //reorder frames in presentation order (to the next I/P frame)
                Lreorder = ReorderDTStoPTS(&vFrame);

                //First pass at fixing PTS values (fixes gross errors only)
                for (MPEG2frame *curFrame = Lreorder.first();
                     curFrame; curFrame = Lreorder.next())
                {
                    poq.UpdateOrigPTS(vid_id, origvPTS,curFrame->pkt);
                    InitialPTSFixup(curFrame, origvPTS, PTSdiscrep, 
                                    maxframes, true);
                }
                // if there was a PTS jump, find the largest change
                // in the next x frames
                // At the end of this, vFrame should look just like it did
                // beforehand
                if (PTSdiscrep && ! file_end)
                {
                    int pos = vFrame.count();
                    int count = Lreorder.count();
                    while (vFrame.count() - frame_pos - count < 20 && !file_end)
                        if ((ret = GetFrame(&pkt)) < 0)
                            return ret;
                    if (! file_end)
                    {
                        int64_t tmp_origvPTS = origvPTS;
                        int numframes = (maxframes > 1) ? maxframes - 1 : 1;
                        bool done = false;
                        while (!done &&
                               (uint)(frame_pos + count + 1) < vFrame.count())
                        {
                            Q3PtrList<MPEG2frame> tmpReorder;
                            vFrame.at(frame_pos + count);
                            tmpReorder = ReorderDTStoPTS(&vFrame);
                            for (MPEG2frame *curFrame = tmpReorder.first();
                                 curFrame; curFrame = tmpReorder.next())
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
                                if (tmpPTSdiscrep != (int64_t)AV_NOPTS_VALUE &&
                                    tmpPTSdiscrep != PTSdiscrep)
                                    PTSdiscrep = tmpPTSdiscrep;
                            }
                            count += tmpReorder.count();
                        }
                    }
                    // push frames onto 'unreadFrames' queue
                    vFrame.at(pos);
                    while (vFrame.at() == pos)
                    {
                        unreadFrames.enqueue(vFrame.current());
                        vFrame.remove();
                    }
                    file_end = false;
                }
  
                //check for cutpoints and convert to I-frames if needed 
                for (MPEG2frame *curFrame = Lreorder.first();
                     curFrame; curFrame = Lreorder.next())
                {
                    if (saveMap.count())
                    {
                        if (saveMap.begin().key() <= frame_count)
                           saveMap.remove(saveMap.begin());
                        if (saveMap.count() && saveMap.begin().value() == 0)
                        {
                            VERBOSE(MPF_IMPORTANT, QString("Saving frame #%1")
                                               .arg(frame_count));
                            if(GetFrameTypeT(curFrame) != 'I')
                                if (ConvertToI(&Lreorder, frame_pos))
                                  return TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR;
                            WriteFrame(QString("save%1.yuv").arg(frame_count),
                                       curFrame);
                        }
                    }
                    if (delMap.count() && delMap.begin().key() <= frame_count)
                    {
                        new_discard_state = delMap.begin().value();

                        delMap.remove(delMap.begin());
                        markedFrameP = curFrame;

                        if (!new_discard_state)
                        {
                            cutEndPTS = markedFrameP->pkt.pts;
                            poq.SetNextPTS(
                                      diff2x33(cutEndPTS, expectedvPTS / 300),
                                      cutEndPTS);
                        }
                        else
                        {
                            cutStartPTS = add2x33(markedFrameP->pkt.pts,
                                          ptsIncrement * 
                                          GetNbFields(markedFrameP) / 2);
                            QMap<int, Q3PtrList<MPEG2frame> >::iterator it;
                            for (it = aFrame.begin(); it != aFrame.end(); it++)
                            {
                                cutState[it.key()] = 1;
                            }
                        }

                        //Rebuild when 'B' frame, or completing a cut, and the marked
                        //frame is a 'P' frame.
                        //After conversion, frames will be in linear order.
                        if ((GetFrameTypeT(curFrame) == 'B') ||
                            (!new_discard_state &&
                             (GetFrameTypeT(curFrame) == 'P')))
                        {
                            if (ConvertToI(&Lreorder, frame_pos))
                                return TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR;
                            ptsorder_eq_dtsorder = true;
                        }
                        else if (! new_discard_state &&
                                 GetFrameTypeT(curFrame) == 'I')
                        {
                            vFrame.remove(frame_pos);
                            vFrame.insert(frame_pos + Lreorder.at(), curFrame);
                            ptsorder_eq_dtsorder = true;
                        }

                        //convert from presentation-order to decode-order
                        markedFrame = vFrame.at(frame_pos + Lreorder.at());

                        if (! new_discard_state)
                        {
                            AddSequence(markedFrame, seqFrame);
                            RenumberFrames(frame_pos + Lreorder.at(),
                                            - GetFrameNum(markedFrame));
                        }
                    }

                    frame_count++;
                }
                lastRealvPkt = Lreorder.getLast()->pkt;

                if (markedFrame || ! discard)
                {
                    //check for PTS discontinuity
                    for (MPEG2frame *curFrame = Lreorder.first();
                         curFrame; curFrame = Lreorder.next())
                    {
                        if (markedFrameP && discard)
                        {
                            if (curFrame != markedFrameP)
                                continue;

                            markedFrameP = NULL;
                        }

                        dec2x33(&curFrame->pkt.pts,
                              poq.Get(vid_id, &curFrame->pkt));
                        deltaPTS = diff2x33(curFrame->pkt.pts,
                                            expectedvPTS / 300);

                        if (deltaPTS < -2 || deltaPTS > 2)
                        {
                            VERBOSE(MPF_PROCESS, QString("PTS discrepancy: "
                                       "%1 != %2 on %3-Type (%4)")
                                       .arg(curFrame->pkt.pts)
                                       .arg(expectedvPTS / 300)
                                       .arg(GetFrameTypeT(curFrame))
                                       .arg(GetFrameNum(curFrame)));
                        }

                        //remove repeat_first_field if necessary
                        if (no_repeat)
                        {
                            SetRepeat(curFrame, 2, 0);
                        }

                        //force PTS to stay in sync (this could be a bad idea!)
                        if (fix_PTS)
                        {
                            curFrame->pkt.pts = expectedvPTS / 300;
                        }

                        if (deltaPTS > ptsIncrement*maxframes)
                        {
                            VERBOSE(MPF_IMPORTANT, QString(
                                    "Need to insert %1 frames > max allowed:"
                                    " %2.  Assuming bad PTS\n")
                                    .arg((int)(deltaPTS / ptsIncrement))
                                    .arg( maxframes));
                            curFrame->pkt.pts = expectedvPTS / 300;
                            deltaPTS = 0;
                        }

                        lastPTS = expectedvPTS;
                        expectedvPTS += 150 * ptsIncrement *
                                        GetNbFields(curFrame);

                        if (curFrame == markedFrameP && new_discard_state)
                            break;
                    }

                    //dtsExtra is applied at the end of this block if the current
                    //tail has repeat_first_field set
                    if (ptsorder_eq_dtsorder)
                        dtsExtra = 0;
                    else
                        dtsExtra = 150 * ptsIncrement *
                                   (GetNbFields(vFrame.at(frame_pos)) - 2);

                    if (! markedFrame && deltaPTS > (4*ptsIncrement / 5))
                    {
                        //if we are off by more than 1/2 frame, it is time to add a frame
                        // The frame(s) will be added right after lVpkt_tail, and
                        // lVpkt_head will be adjusted accordingly


                        vFrame.at(frame_pos)->pkt.pts = lastPTS / 300;
                        int ret = InsertFrame(GetFrameNum(vFrame.current()),
                                               deltaPTS, ptsIncrement, 0);
                        if(ret < 0)
                            return TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR;
                        for (vFrame.at(frame_pos + Lreorder.count()); ret;
                             vFrame.next(), --ret)
                        {
                            lastPTS = expectedvPTS;
                            expectedvPTS += 150 * ptsIncrement *
                                            GetNbFields(vFrame.current());
                            Lreorder.append(vFrame.current());
                        }
                    }

                    //Set DTS (ignore any current values), and send frame to
                    //multiplexer
                    vFrame.at(frame_pos);

                    for (uint i = 0; i < Lreorder.count(); i++, vFrame.next())
                    {
                        if (discard)
                        {
                            if (markedFrame != vFrame.current())
                                continue;

                            discard = false;

                            markedFrame = NULL;
                        }

                        vFrame.current()->pkt.dts = (expectedDTS / 300);
                        if (GetFrameTypeT(vFrame.current()) == 'B')
                            vFrame.current()->pkt.pts = (expectedDTS / 300);
                        expectedDTS += 150 * ptsIncrement *
                                       ((! ptsorder_eq_dtsorder && i == 0) ?
                                        2 : GetNbFields(vFrame.current()));
                        VERBOSE(MPF_FRAME,QString("VID: %1 #:%2 nb: %3"
                                " pts: %4 dts: %5 pos: %6")
                                .arg(GetFrameTypeT(vFrame.current()))
                                .arg(GetFrameNum(vFrame.current()))
                                .arg(GetNbFields(vFrame.current()))
                                .arg(PtsTime(vFrame.current()->pkt.pts))
                                .arg(PtsTime(vFrame.current()->pkt.dts))
                                .arg(vFrame.current()->pkt.pos));
                        if (AddFrame(vFrame.current()))
                            return TRANSCODE_BUGGY_EXIT_DEADLOCK;

                        if (vFrame.current() == markedFrame)
                        {
                            markedFrame = NULL;
                            discard = true;
                        }
                    }
                        
                    expectedDTS += dtsExtra;
                }
                else
                {
                    vFrame.at(frame_pos + Lreorder.count());
                }
                if (PTSdiscrep)
                    poq.SetNextPos(add2x33(poq.Get(vid_id, &lastRealvPkt),
                                       PTSdiscrep), lastRealvPkt);
            }

            if (discard)
                cutEndPTS = lastRealvPkt.pts;

            if (file_end)
                use_secondary = false;
            if (vFrame.count() > 1 || file_end)
                StoreSecondary();
        }

        for (QMap<int, Q3PtrList<MPEG2frame> >::iterator it = aFrame.begin();
                it != aFrame.end(); it++)
        {
            Q3PtrList<MPEG2frame> *af = &(*it);
            AVCodecContext *CC = getCodecContext(it.key());
            bool backwardsPTS = false;

            while (af->count())
            {
                // What to do if the CC is corrupt?
                // Just wait and hope it repairs itself
                if (CC->sample_rate == 0 || CC->frame_size == 0)
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
                int64_t nextPTS, tmpPTS;
                int64_t incPTS =
                         90000LL * (int64_t)CC->frame_size / CC->sample_rate;

                if (poq.UpdateOrigPTS(it.key(), origaPTS[it.key()],
                                                  af->first()->pkt) < 0)
                {
                    backwardsPTS = true;
                    af_dlta_cnt[it.key()] = 0;
                }

                tmpPTS = diff2x33(af->first()->pkt.pts,
                                  origaPTS[it.key()] / 300);
                if (tmpPTS < -incPTS)
                {
#ifdef DEBUG_AUDIO
                    VERBOSE(MPF_PROCESS, QString("Aud discard: PTS %1 < %2")
                            .arg(PtsTime(af->first()->pkt.pts))
                            .arg(PtsTime(origaPTS[it.key()] / 300)));
#endif
                    framePool.enqueue(af->first());
                    af->remove();
                    af_dlta_cnt[it.key()] = 0;
                    continue;
                }
                if (tmpPTS > incPTS * maxframes)
                {
                        VERBOSE(MPF_PROCESS,
                           QString("Found invalid audio PTS (off by %1) at %2")
                                   .arg(PtsTime(tmpPTS))
                                   .arg(PtsTime(origaPTS[it.key()] / 300)));
                        if (backwardsPTS && tmpPTS < 90000LL)
                        {
                            //there are missing audio frames
                            VERBOSE(MPF_PROCESS,
                                    QString("Fixing missing audio frames"));
                            ptsinc((uint64_t *)&origaPTS[it.key()],
                                   300 * tmpPTS);
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
                                af_dlta_cnt[it.key()]++;
                        }
                        af->first()->pkt.pts = origaPTS[it.key()] / 300;
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
                nextPTS = add2x33(af->first()->pkt.pts,
                           90000LL * (int64_t)CC->frame_size / CC->sample_rate);

                if ((cutState[it.key()] == 1 &&
                     cmp2x33(nextPTS, cutStartPTS) > 0) ||
                    (cutState[it.key()] == 2 && 
                     cmp2x33(af->first()->pkt.pts, cutEndPTS) < 0))
                {
#ifdef DEBUG_AUDIO
                    VERBOSE(MPF_PROCESS, QString("Aud in cutpoint:\n"
                                         "\t%1 > %2 &&\n"
                                         "\t%3 < %4")
                        .arg(PtsTime(nextPTS)).arg(PtsTime(cutStartPTS))
                        .arg(PtsTime(af->first()->pkt.pts))
                        .arg(PtsTime(cutEndPTS)));
#endif
                    framePool.enqueue(af->first());
                    af->remove();
                    cutState[it.key()] = 2;
                    ptsinc((uint64_t *)&origaPTS[it.key()], incPTS * 300);
                    continue;
                }
                int64_t deltaPTS = poq.Get(it.key(), &af->first()->pkt);

                if (udiff2x33(nextPTS, deltaPTS) * 300 > expectedDTS &&
                    cutState[it.key()] != 1)
                {
#ifdef DEBUG_AUDIO
                    VERBOSE(MPF_PROCESS, QString("Aud not ready: %1 > %2")
                            .arg(PtsTime(udiff2x33(nextPTS, deltaPTS)))
                            .arg(PtsTime(expectedDTS / 300)));
#endif
                    break;
                }

                if (cutState[it.key()] == 2)
                    cutState[it.key()] = 0;

                ptsinc((uint64_t *)&origaPTS[it.key()], incPTS * 300);

                dec2x33(&af->first()->pkt.pts, deltaPTS);

                //expectedPTS[it.key()] = udiff2x33(nextPTS, initPTS);
                // write_audio(lApkt_tail->pkt, initPTS);
                VERBOSE(MPF_FRAME, QString("AUD #%1: pts: %2 pos: %3")
                        .arg(it.key()) 
                        .arg(PtsTime(af->current()->pkt.pts))
                        .arg(af->current()->pkt.pos));
                if (AddFrame(af->current()))
                    return TRANSCODE_BUGGY_EXIT_DEADLOCK;
                framePool.enqueue(af->first());

                af->remove();
            }
        }

        if (file_end)
            break;
    }

    rx.done = 1;
    pthread_mutex_lock( &rx.mutex );
    pthread_cond_signal(&rx.cond);
    pthread_mutex_unlock( &rx.mutex );
    pthread_join(thread, NULL);

    av_close_input_file(inputFC);
    inputFC = NULL;
    return REENCODE_OK;
}

#ifdef NO_MYTH
int print_verbose_messages = MPF_GENERAL | MPF_IMPORTANT;

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
    char *infile = NULL, *outfile = NULL, *format = NULL;
    int no_repeat = 0, fix_PTS = 0, max_frames = 20, otype = REPLEX_MPEG2;
    bool showprogress = 0;
    const struct option long_options[] =
        {
            {"infile", required_argument, NULL, 'i'},

            {"outfile", required_argument, NULL, 'o'},
            {"format", required_argument, NULL, 'r'},
            {"dbg_lvl", required_argument, NULL, 'd'},
            {"cutlist", required_argument, NULL, 'c'},
            {"saveframe", required_argument, NULL, 's'},
            {"ostream", required_argument, NULL, 'e'},
            {"no3to2", no_argument, NULL, 't'},
            {"fixup", no_argument, NULL, 'f'},
            {"showprogress", no_argument, NULL, 'p'},
            {"help", no_argument , NULL, 'h'},
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
                print_verbose_messages = atoi(optarg);
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

    if (infile == NULL || outfile == NULL)
        usage(argv[0]);

    MPEG2fixup m2f(infile, outfile, NULL, format, 
                   no_repeat, fix_PTS, max_frames,
                   showprogress, otype);

    if (cutlist.count())
        m2f.AddRangeList(cutlist, MPF_TYPE_CUTLIST);
    if (savelist.count())
        m2f.AddRangeList(savelist, MPF_TYPE_SAVELIST);
    return m2f.Start();
}
#endif

int MPEG2fixup::BuildKeyframeIndex(QString &file,
                                   frm_pos_map_t &posMap)
{
    VERBOSE(MPF_GENERAL, "Generating Keyframe Index");

    AVPacket pkt;
    int count = 0;

    /*============ initialise AV ===============*/
    QByteArray fname = file.toLocal8Bit();
    if (!InitAV(fname.constData(), NULL, 0))
        return TRANSCODE_EXIT_UNKNOWN_ERROR;

    if (mkvfile)
    {
        VERBOSE(MPF_IMPORTANT, "Seek tables are not required for MKV");
        return TRANSCODE_EXIT_UNKNOWN_ERROR;
    }

    av_init_packet(&pkt);

    while (av_read_frame(inputFC, &pkt) >= 0)
    {
        if (pkt.stream_index == vid_id)
        {
            if (pkt.flags & PKT_FLAG_KEY)
                posMap[count] = pkt.pos;
            count++;
        }
        av_free_packet(&pkt);
    }

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    VERBOSE(MPF_GENERAL, "Transcode Completed");

    return REENCODE_OK;
}
