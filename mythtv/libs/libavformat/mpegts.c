/*
 * MPEG2 transport stream (aka ATSC/DVB/Cable) demux
 * Copyright (c) 2002-2003 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "avformat.h"
#include "crc.h"
#include <pthread.h>
#include "mpegts.h"

//#define DEBUG_SI
//#define DEBUG_SEEK

/* 1.0 second at 24Mbit/s */
#define MAX_SCAN_PACKETS 32000

/* maximum size in which we look for synchronisation if
   synchronisation is lost */
#define MAX_RESYNC_SIZE 4096

#define PMT_NOT_YET_FOUND 0
#define PMT_NOT_IN_PAT    1
#define PMT_FOUND         2

typedef struct PESContext PESContext;
typedef struct SectionContext SectionContext;

static PESContext* add_pes_stream(MpegTSContext *ts, int pid, int stream_type);
static AVStream* new_pes_av_stream(PESContext *pes, uint32_t code);
static AVStream *new_section_av_stream(SectionContext *sect, uint32_t code);
static SectionContext *add_section_stream(MpegTSContext *ts, int pid, int stream_type);
static void mpegts_cleanup_streams(MpegTSContext *ts);
static int is_desired_stream(int type);
static int find_in_list(const int *pids, int pid);

extern pthread_mutex_t avcodeclock;

enum MpegTSFilterType {
    MPEGTS_PES,
    MPEGTS_SECTION,
};

typedef struct
{
    char language[4];
    int comp_page;
    int anc_page;
    int sub_id;
    int txt_type;
    /* DSMCC data */
    int data_id;
    int carousel_id;
    int component_tag;
} dvb_caption_info_t;

static int mpegts_parse_desc(dvb_caption_info_t *dvbci,
                             uint8_t **p, uint8_t *p_end);

typedef struct
{
    int pid;
    int type;
    dvb_caption_info_t dvbci;
} pmt_entry_t;

static int is_pat_same(MpegTSContext *mpegts_ctx,
                       int *pmt_pnums, int *pmts_pids, uint pmt_count);

static void mpegts_add_stream(MpegTSContext *ts, pmt_entry_t* item);
static int is_pmt_same(MpegTSContext *mpegts_ctx, pmt_entry_t* items,
                       int item_cnt);

typedef void PESCallback(void *opaque, const uint8_t *buf, int len, 
                         int is_start, int64_t startpos);

typedef struct MpegTSPESFilter {
    PESCallback *pes_cb;
    void *opaque;
} MpegTSPESFilter;

typedef void SectionCallback(void *opaque, const uint8_t *buf, int len);

typedef void SetServiceCallback(void *opaque, int ret);

typedef struct MpegTSSectionFilter {
    int section_index;
    int section_h_size;
    uint8_t *section_buf;
    int check_crc:1;
    int end_of_section_reached:1;
    SectionCallback *section_cb;
    void *opaque;
} MpegTSSectionFilter;

typedef struct MpegTSFilter {
    int pid;
    int last_cc; /* last cc code (-1 if first packet) */
    enum MpegTSFilterType type;
    union {
        MpegTSPESFilter pes_filter;
        MpegTSSectionFilter section_filter;
    } u;
} MpegTSFilter;

typedef struct MpegTSService {
    int running:1;
    int sid;    /**< MPEG Program Number of stream */
    int pid;    /**< MPEG Program ID of Stream */
    char *provider_name; /**< DVB Network name, "" if not DVB stream */
    char *name; /**< DVB Service name, "MPEG Program [sid]" if not DVB stream*/
} MpegTSService;

/** maximum number of PMT's we expect to be described in a PAT */
#define PAT_MAX_PMT 128

/** maximum number of streams we expect to be described in a PMT */
#define PMT_PIDS_MAX 256

struct MpegTSContext {
    /* user data */
    AVFormatContext *stream;
    /** raw packet size, including FEC if present            */
    int raw_packet_size;
    /** if true, all pids are analyzed to find streams.      */
    int auto_guess;
#if 0
    int set_service_ret;
#endif

    /** force raw MPEG2 transport stream output, if possible */
    int mpeg2ts_raw;
    /** compute exact PCR for each transport stream packet   */
    int mpeg2ts_compute_pcr;

    int64_t cur_pcr;    /**< used to estimate the exact PCR */
    int pcr_incr;       /**< used to estimate the exact PCR */
    int pcr_pid;        /**< used to estimate the exact PCR */
    
    /* variables used for finding initial PAT and PMT        */

    /** if set, stop_parse is set when PAT/PMT is found      */
    int scanning;
    /** stop parsing loop                                    */
    int stop_parse;
    /** set to PMT_NOT_IN_PAT in pat_cb when scanning is true
     *  and the MPEG program number "req_sid" is not found in PAT;
     *  set to PMT_FOUND when a PMT with a the "req_sid" program 
     *  number is found. */
    int pmt_scan_state;

    /** packet containing Audio/Video data */
    AVPacket *pkt;

    /******************************************/
    /* private mpegts data */

#if 0
    /* scan context */
    MpegTSFilter *sdt_filter;
#endif

    /** number of PMTs in the last PAT seen */
    int nb_services;
    /** list of PMTs in the last PAT seen */
    MpegTSService **services;

#if 0
    /* set service context (XXX: allocated it ?) */
    SetServiceCallback *set_service_cb;
    void *set_service_opaque;
#endif

    /** filter for the PAT */
    MpegTSFilter *pat_filter;
    /** filter for the PMT for the MPEG program number specified by req_sid */
    MpegTSFilter *pmt_filter;
    /** MPEG program number of stream we want to decode */
    int req_sid;

    /** filters for various streams specified by PMT + for the PAT and PMT */
    MpegTSFilter *pids[NB_PID_MAX];

    /** number of streams in the last PMT seen */
    int pid_cnt;
    /** list of streams in the last PMT seen */
    int pmt_pids[PMT_PIDS_MAX];
};

/* TS stream handling */

enum MpegTSState {
    MPEGTS_HEADER = 0,
    MPEGTS_PESHEADER_FILL,
    MPEGTS_PAYLOAD,
    MPEGTS_SKIP,
};

/* enough for PES header + length */
#define PES_START_SIZE 9
#define MAX_PES_HEADER_SIZE (9 + 255)

struct PESContext {
    int pid;
    int stream_type;
    MpegTSContext *ts;
    AVFormatContext *stream;
    AVStream *st;
    enum MpegTSState state;
    /* used to get the format */
    int data_index;
    int total_size;
    int pes_header_size;
    int64_t pts, dts;
    uint8_t header[MAX_PES_HEADER_SIZE];
    int64_t startpos;
};

struct SectionContext {
    int pid;
    int stream_type;
    MpegTSContext *ts;
    AVFormatContext *stream;
    AVStream *st;
};

/** \fn write_section_data(AVFormatContext*,MpegTSFilter*,const uint8_t*,int,int)
 *  Assembles PES packets out of TS packets, and then calls the "section_cb"
 *  function when they are complete.
 *
 *  NOTE: "DVB Section" is DVB terminology for an MPEG PES packet.
 */
static void write_section_data(AVFormatContext *s, MpegTSFilter *tss1,
                               const uint8_t *buf, int buf_size, int is_start)
{
    MpegTSSectionFilter *tss = &tss1->u.section_filter;
    int len;
    
    if (is_start) {
        memcpy(tss->section_buf, buf, buf_size);
        tss->section_index = buf_size;
        tss->section_h_size = -1;
        tss->end_of_section_reached = 0;
    } else {
        if (tss->end_of_section_reached)
            return;
        len = 4096 - tss->section_index;
        if (buf_size < len)
            len = buf_size;
        memcpy(tss->section_buf + tss->section_index, buf, len);
        tss->section_index += len;
    }

    while (1) { /* There may be several tables in this data. */
        /* compute section length if possible */
        if (tss->section_h_size == -1 && tss->section_index >= 3) {
            len = (((tss->section_buf[1] & 0xf) << 8) | tss->section_buf[2]) + 3;
            if (len > 4096)
                break;
            tss->section_h_size = len;
        }

        if (tss->section_h_size == -1 || tss->section_index < tss->section_h_size)
            break;

        if (!tss->check_crc || av_crc(av_crc04C11DB7, -1, tss->section_buf, tss->section_h_size) == 0)
            tss->section_cb(tss->opaque, tss->section_buf, tss->section_h_size);

        if (tss->section_index > tss->section_h_size) {
            int left = tss->section_index - tss->section_h_size;
            memmove(tss->section_buf, tss->section_buf+tss->section_h_size, left);
            tss->section_index = left;
            tss->section_h_size = -1;
        } else {
            tss->end_of_section_reached = 1;
            break;
        }
    }
}

void mpegts_close_filter(MpegTSContext *ts, MpegTSFilter *filter)
{
    int pid;

    if (!ts || !filter)
        return;
    pid = filter->pid;

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "Closing Filter: pid=0x%x\n", pid);
#endif
    if (filter == ts->pmt_filter)
    {
        av_log(NULL, AV_LOG_DEBUG, "Closing PMT Filter: pid=0x%x\n", pid);
        ts->pmt_filter = NULL;
    }
    if (filter == ts->pat_filter)
    {
        av_log(NULL, AV_LOG_DEBUG, "Closing PAT Filter: pid=0x%x\n", pid);
        ts->pat_filter = NULL;
    }

    if (filter->type == MPEGTS_SECTION)
        av_freep(&filter->u.section_filter.section_buf);
    else if (filter->type == MPEGTS_PES)
        av_freep(&filter->u.pes_filter.opaque);

    av_free(filter);
    ts->pids[pid] = NULL;
}


MpegTSFilter *mpegts_open_section_filter(MpegTSContext *ts, unsigned int pid, 
                                         SectionCallback *section_cb, void *opaque,
                                         int check_crc)

{
    MpegTSFilter *filter = ts->pids[pid];
    MpegTSSectionFilter *sec;
    
#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "Opening Filter: pid=0x%x\n", pid);
#endif

    if (NULL!=filter) {
#ifdef DEBUG_SI
	av_log(NULL, AV_LOG_DEBUG, "Filter Already Exists\n");
#endif
        mpegts_close_filter(ts, filter);
    }

    if (pid >= NB_PID_MAX || ts->pids[pid])
        return NULL;
    filter = av_mallocz(sizeof(MpegTSFilter));
    if (!filter) 
        return NULL;
    ts->pids[pid] = filter;
    filter->type = MPEGTS_SECTION;
    filter->pid = pid;
    filter->last_cc = -1;
    sec = &filter->u.section_filter;
    sec->section_cb = section_cb;
    sec->opaque = opaque;
    sec->section_buf = av_malloc(MAX_SECTION_SIZE);
    sec->check_crc = check_crc;
    if (!sec->section_buf) {
        av_free(filter);
        return NULL;
    }
    return filter;
}

MpegTSFilter *mpegts_open_pes_filter(MpegTSContext *ts, unsigned int pid, 
                                     PESCallback *pes_cb,
                                     void *opaque)
{
    MpegTSFilter *filter;
    MpegTSPESFilter *pes;

    if (pid >= NB_PID_MAX || ts->pids[pid])
        return NULL;
    filter = av_mallocz(sizeof(MpegTSFilter));
    if (!filter) 
        return NULL;
    ts->pids[pid] = filter;
    filter->type = MPEGTS_PES;
    filter->pid = pid;
    filter->last_cc = -1;
    pes = &filter->u.pes_filter;
    pes->pes_cb = pes_cb;
    pes->opaque = opaque;
    return filter;
}


static int analyze(const uint8_t *buf, int size, int packet_size, int *index){
    int stat[packet_size];
    int i;
    int x=0;
    int best_score=0;

    memset(stat, 0, packet_size*sizeof(int));

    for(x=i=0; i<size; i++){
        if(buf[i] == 0x47){
            stat[x]++;
            if(stat[x] > best_score){
                best_score= stat[x];
                if(index) *index= x;
            }
        }

        x++;
        if(x == packet_size) x= 0;
    }

    return best_score;
}

/* autodetect fec presence. Must have at least 1024 bytes  */
static int get_packet_size(const uint8_t *buf, int size)
{
    int score, fec_score, dvhs_score;

    if (size < (TS_FEC_PACKET_SIZE * 5 + 1))
        return -1;
        
    score    = analyze(buf, size, TS_PACKET_SIZE, NULL);
    dvhs_score    = analyze(buf, size, TS_DVHS_PACKET_SIZE, NULL);
    fec_score= analyze(buf, size, TS_FEC_PACKET_SIZE, NULL);
/*av_log(NULL, AV_LOG_DEBUG, "score: %d, fec_score: %d \n", score, fec_score);*/
    
    if     (score > fec_score && score > dvhs_score) return TS_PACKET_SIZE;
    else if(dvhs_score > score && dvhs_score > fec_score) return TS_DVHS_PACKET_SIZE;
    else if(score < fec_score && dvhs_score < fec_score) return TS_FEC_PACKET_SIZE;
    else                       return -1;
}

typedef struct SectionHeader {
    uint8_t tid;
    uint16_t id;
    uint8_t version;
    uint8_t sec_num;
    uint8_t last_sec_num;
} SectionHeader;

static inline int get8(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if (p >= p_end)
        return -1;
    c = *p++;
    *pp = p;
    return c;
}

static inline int get16(const uint8_t **pp, const uint8_t *p_end)
{
    const uint8_t *p;
    int c;

    p = *pp;
    if ((p + 1) >= p_end)
        return -1;
    c = (p[0] << 8) | p[1];
    p += 2;
    *pp = p;
    return c;
}

/* read and allocate a DVB string preceeded by its length */
static char *getstr8(const uint8_t **pp, const uint8_t *p_end)
{
    int len;
    const uint8_t *p;
    char *str;

    p = *pp;
    len = get8(&p, p_end);
    if (len < 0)
        return NULL;
    if ((p + len) > p_end)
        return NULL;
    str = av_malloc(len + 1);
    if (!str)
        return NULL;
    memcpy(str, p, len);
    str[len] = '\0';
    p += len;
    *pp = p;
    return str;
}

static int parse_section_header(SectionHeader *h, 
                                const uint8_t **pp, const uint8_t *p_end)
{
    int val;

    val = get8(pp, p_end);
    if (val < 0)
        return -1;
    h->tid = val;
    *pp += 2;
    val = get16(pp, p_end);
    if (val < 0)
        return -1;
    h->id = val;
    val = get8(pp, p_end);
    if (val < 0)
        return -1;
    h->version = (val >> 1) & 0x1f;
    val = get8(pp, p_end);
    if (val < 0)
        return -1;
    h->sec_num = val;
    val = get8(pp, p_end);
    if (val < 0)
        return -1;
    h->last_sec_num = val;

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "sid=0x%x sec_num=%d/%d\n",
           h->id, h->sec_num, h->last_sec_num);
#endif
    return 0;
}

static MpegTSService *new_service(MpegTSContext *ts, int sid, int pid,
                                  char *provider_name, char *name)
{
    MpegTSService *service;

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "new_service: sid=0x%04x provider='%s' name='%s'\n", 
           sid, provider_name, name);
#endif

    service = av_mallocz(sizeof(MpegTSService));
    if (!service)
        return NULL;
    service->sid = sid;
    service->pid = pid;
    service->provider_name = provider_name;
    service->name = name;
    dynarray_add(&ts->services, &ts->nb_services, service);
    return service;
}

static int mpegts_parse_program_info_length(uint8_t **p, uint8_t *p_end)
{
    int program_info_length = get16(p, p_end);
    if (program_info_length < 0)
        return -1;
    program_info_length &= 0xfff;
    *p += program_info_length;
    if (*p >= p_end)
        return -1;
    return program_info_length;
}

static int find_in_list(const int *pids, int pid) {
    int i;
    for (i=0; i<PMT_PIDS_MAX; i++)
        if (pids[i]==pid)
            return i;
    return -1;
}

static int mpegts_parse_pcrpid(MpegTSContext *mpegts_ctx,
                               uint8_t **p, uint8_t *p_end)
{
    int pcr_pid = get16(p, p_end);
    if (pcr_pid < 0)
        return -1;
    pcr_pid &= 0x1fff;
    mpegts_ctx->pcr_pid = pcr_pid;
#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "pcr_pid=0x%x\n", pcr_pid);
#endif
    return pcr_pid;
}

#define HANDLE_PMT_ERROR(MSG) \
    do { av_log(NULL, AV_LOG_ERROR, MSG); return; } while (0)

#define HANDLE_PMT_PARSE_ERROR(PMSG) \
    HANDLE_PMT_ERROR("Something went terribly wrong in PMT parsing" \
                     " when looking at " PMSG "\n")

static void pmt_cb(void *opaque, const uint8_t *section, int section_len)
{
    MpegTSContext *mpegts_ctx = opaque;
    const uint8_t *p = section, *p_end = section + section_len - 4;
    SectionHeader header;

    int last_item = 0;
    pmt_entry_t items[PMT_PIDS_MAX];
    bzero(&items, sizeof(pmt_entry_t) * PMT_PIDS_MAX);

    mpegts_cleanup_streams(mpegts_ctx); /* in case someone else removed streams.. */

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "PMT: len %i\n", section_len);
    av_hex_dump(stdout, (uint8_t *)section, section_len);
#endif

    if (parse_section_header(&header, &p, p_end) < 0)
        HANDLE_PMT_PARSE_ERROR("section header");

    /* if we require a specific PMT, and this isn't it return silently */
    if (mpegts_ctx->req_sid >= 0 && header.id != mpegts_ctx->req_sid)
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "We are looking for program 0x%x, not 0x%x",
               mpegts_ctx->req_sid, header.id);
#endif
        return;
    }

    /* Check if this is really a PMT, and if so the right one */
    if (header.tid != PMT_TID)
        HANDLE_PMT_ERROR("pmt_cb() got a TS packet that doesn't have PMT TID!");

    /* Extract the Program Clock Reference PID */
    if (mpegts_parse_pcrpid(mpegts_ctx, &p, p_end) < 0)
        HANDLE_PMT_PARSE_ERROR("PCR PID");

    /* Extract program info length, just so we can skip it */
    if (mpegts_parse_program_info_length(&p, p_end) < 0)
        HANDLE_PMT_PARSE_ERROR("program info");

    /* parse new streams */
    while (p < p_end)
    {
        dvb_caption_info_t dvbci;
        int stream_type = get8(&p, p_end);
        int pid = get16(&p, p_end);
        int desc_ok = mpegts_parse_desc(&dvbci, &p, p_end);
        if ((stream_type < 0) || (pid < 0) || (desc_ok < 0))
        {
            av_log(NULL, AV_LOG_ERROR,
                   "Something went terribly wrong in PMT parsing\n"
                   "    when looking at descriptors (0x%x 0x%x)\n",
                   stream_type, pid & 0x1fff);
            break;
        }
        pid &= 0x1fff;

        if (dvbci.sub_id && (stream_type == STREAM_TYPE_PRIVATE_DATA))
            stream_type = STREAM_TYPE_SUBTITLE_DVB;

        if (dvbci.txt_type && (stream_type == STREAM_TYPE_PRIVATE_DATA))
            stream_type = STREAM_TYPE_VBI_DVB;
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "stream_type=%d pid=0x%x\n", stream_type, pid);
#endif

        if (is_desired_stream(stream_type))
        {
            /* Add it to the list unless we're out of space. */
            if (last_item < PMT_PIDS_MAX)
            {
                items[last_item].pid  = pid;
                items[last_item].type = stream_type;
                memcpy(&items[last_item].dvbci, &dvbci,
                       sizeof(dvb_caption_info_t));
                last_item++;
            }
            else
            {
                av_log(NULL, AV_LOG_DEBUG,
                       "Could not add new pid 0x%x, i = %i, "
                       "would cause overrun\n", pid, last_item);
                assert(0);
            }
        }
    }

    /* if the pmt has changed delete old streams,
     * create new ones, and notify any listener.
     */
    if (!is_pmt_same(mpegts_ctx, items, last_item))
    {
        AVFormatContext *avctx = mpegts_ctx->stream;
        int idx;
        /* flush out old AVPackets */
        av_read_frame_flush(avctx);
        /* delete old streams */
        for (idx = mpegts_ctx->pid_cnt-1; idx>=0; idx--)
            av_remove_stream(mpegts_ctx->stream, mpegts_ctx->pmt_pids[idx], 1);

        /* create new streams */
        for (idx = 0; idx < last_item; idx++)
            mpegts_add_stream(mpegts_ctx, &items[idx]);

        /* cache pmt */
        void *tmp0 = avctx->cur_pmt_sect;
        void *tmp1 = av_malloc(section_len);
        memcpy(tmp1, section, section_len);
        avctx->cur_pmt_sect = (uint8_t*) tmp1;
        avctx->cur_pmt_sect_len = section_len;
        if (tmp0)
            av_free(tmp0);

        /* notify stream_changed listeners */
        if (avctx->streams_changed)
        {
            av_log(NULL, AV_LOG_DEBUG, "streams_changed()\n");
            avctx->streams_changed(avctx->stream_change_data);
        } 
    }

    /* if we are scanning, tell scanner we found the PMT */
    if (mpegts_ctx->scanning)
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "Found PMT, ending scan\n");
#endif
        mpegts_ctx->pmt_scan_state = PMT_FOUND;
        mpegts_ctx->stop_parse = 1;
    }
}

static int is_pat_same(MpegTSContext *mpegts_ctx,
                       int *pmt_pnums, int *pmt_pids, uint pmt_count)
{
    int idx;
    if (mpegts_ctx->nb_services != pmt_count)
        return 0;

    for (idx = 0; idx < pmt_count; idx++)
    {
        if ((mpegts_ctx->services[idx]->sid != pmt_pnums[idx]) ||
            (mpegts_ctx->services[idx]->pid != pmt_pids[idx]))
            return 0;
    }
    return 1;
}

static int is_pmt_same(MpegTSContext *mpegts_ctx,
                       pmt_entry_t* items, int item_cnt)
{
    int idx;
    if (mpegts_ctx->pid_cnt != item_cnt)
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "mpegts_ctx->pid_cnt=%d != item_cnt=%d\n",
               mpegts_ctx->pid_cnt, item_cnt);
#endif
        return 0;
    }
    for (idx = 0; idx < item_cnt; idx++)
    {
        /* check for pid */
        int loc = find_in_list(mpegts_ctx->pmt_pids, items[idx].pid);
        if (loc < 0)
        {
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG,
                   "find_in_list(..,[%d].pid=%d) => -1\n"
                   "is_pmt_same() => false\n",
                   idx, items[idx].pid);
#endif
            return 0;
        }

        /* check stream type */
        MpegTSFilter *tss = mpegts_ctx->pids[items[idx].pid];
        if (!tss)
        {
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG,
                   "mpegts_ctx->pids[items[%d].pid=%d] => null\n"
                   "is_pmt_same() => false\n",
                   idx, items[idx].pid);
#endif
            return 0;
        }
        if (tss->type == MPEGTS_PES)
        {
            PESContext *pes = (PESContext*) tss->u.pes_filter.opaque;
            if (!pes)
            {
#ifdef DEBUG_SI
                av_log(NULL, AV_LOG_DEBUG, "pes == null, where idx %d\n"
                       "is_pmt_same() => false\n", idx);
#endif
                return 0;
            }
            if (pes->stream_type != items[idx].type)
            {
#ifdef DEBUG_SI
                av_log(NULL, AV_LOG_DEBUG,
                       "pes->stream_type != items[%d].type\n"
                       "is_pmt_same() => false\n", idx);
#endif
                return 0;
            }
        }
        else if (tss->type == MPEGTS_SECTION)
        {
            SectionContext *sect = (SectionContext*) tss->u.section_filter.opaque;
            if (!sect)
            {
#ifdef DEBUG_SI
                av_log(NULL, AV_LOG_DEBUG, "sect == null, where idx %d\n"
                       "is_pmt_same() => false\n", idx);
#endif
                return 0;
            }
            if (sect->stream_type != items[idx].type)
            {
#ifdef DEBUG_SI
                av_log(NULL, AV_LOG_DEBUG,
                       "sect->stream_type != items[%d].type\n"
                       "is_pmt_same() => false\n", idx);
#endif
                return 0;
            }            
        }
        else
        {
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG,
                   "tss->type != MPEGTS_PES, where idx %d\n"
                   "is_pmt_same() => false\n", idx);
#endif
            return 0;
        }
    }
#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "is_pmt_same() => true\n", idx);
#endif
    return 1;
}

static int is_desired_stream(int type)
{
    int val = 0;
    switch (type)
    {
        case STREAM_TYPE_AUDIO_MPEG1:
        case STREAM_TYPE_AUDIO_MPEG2:
        case STREAM_TYPE_VIDEO_MPEG1:
        case STREAM_TYPE_VIDEO_MPEG2:
        case STREAM_TYPE_VIDEO_MPEG4:
        case STREAM_TYPE_VIDEO_H264:
        case STREAM_TYPE_AUDIO_AAC:
        case STREAM_TYPE_AUDIO_AC3:
        case STREAM_TYPE_AUDIO_DTS:
        case STREAM_TYPE_PRIVATE_DATA:
        case STREAM_TYPE_VBI_DVB:
        case STREAM_TYPE_SUBTITLE_DVB:
        case STREAM_TYPE_DSMCC_B:
            val = 1;
            break;
        default:
            break;
    }    
    return val;
}

static int mpegts_parse_desc(dvb_caption_info_t *dvbci,
                             uint8_t **p, uint8_t *p_end)
{
    const uint8_t *desc_list_end, *desc_end;
    int desc_list_len, desc_len, desc_tag;

    bzero(dvbci, sizeof(dvb_caption_info_t));
    dvbci->component_tag = -1;

    desc_list_len = get16(p, p_end);
    if (desc_list_len < 0)
        return -1;
    desc_list_len &= 0xfff;
    desc_list_end = *p + desc_list_len;
    if (desc_list_end > p_end)
        return -1;
    while (*p < desc_list_end)
    {
        desc_tag = get8(p, desc_list_end);
        if (desc_tag < 0)
            break;
        desc_len = get8(p, desc_list_end);
        desc_end = *p + desc_len;
        if (desc_end > desc_list_end)
            break;
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "tag: 0x%02x len=%d\n", desc_tag, desc_len);
#endif
        switch (desc_tag)
        {
            case DVB_SUBT_DESCID:
                dvbci->language[0] = get8(p, desc_end);
                dvbci->language[1] = get8(p, desc_end);
                dvbci->language[2] = get8(p, desc_end);
                dvbci->language[3] = 0;
                get8(p, desc_end);
                dvbci->comp_page   = get16(p, desc_end);
                dvbci->anc_page    = get16(p, desc_end);
                dvbci->sub_id = (dvbci->anc_page << 16) | dvbci->comp_page;
                break;
            case 0x0a: /* ISO 639 language descriptor */
                dvbci->language[0] = get8(p, desc_end);
                dvbci->language[1] = get8(p, desc_end);
                dvbci->language[2] = get8(p, desc_end);
                dvbci->language[3] = 0;
                break;
            case DVB_BROADCAST_ID:
                dvbci->data_id = get16(p, desc_end);
                break;
            case DVB_CAROUSEL_ID:
                {
                    int carId = 0;
                    carId = get8(p, desc_end);
                    carId = (carId << 8) | get8(p, desc_end);
                    carId = (carId << 8) | get8(p, desc_end);
                    carId = (carId << 8) | get8(p, desc_end);
                    dvbci->carousel_id = carId;
                }
                break;
            case DVB_DATA_STREAM:
                dvbci->component_tag = get8(p, desc_end);
                break;
            case DVB_VBI_DESCID:
                dvbci->language[0] = get8(p, desc_end);
                dvbci->language[1] = get8(p, desc_end);
                dvbci->language[2] = get8(p, desc_end);
                dvbci->txt_type = (get8(p, desc_end)) >> 3;
                break;
            default:
                break;
        }
        *p = desc_end;
    }
    *p = desc_list_end;
    return 1;
}

static void mpegts_cleanup_streams(MpegTSContext *ts)
{
    int i;
    int orig_pid_cnt = ts->pid_cnt;
    for (i=0; i<ts->pid_cnt; i++)
    {
        if (!ts->pids[ts->pmt_pids[i]])
        {
            mpegts_remove_stream(ts, ts->pmt_pids[i]);
            i--;
        }
    }
    if (orig_pid_cnt != ts->pid_cnt)
    {
        av_log(NULL, AV_LOG_DEBUG,
               "mpegts_cleanup_streams: pid_cnt bfr %d aft %d\n",
               orig_pid_cnt, ts->pid_cnt);
    }
}

static void mpegts_add_stream(MpegTSContext *ts, pmt_entry_t* item)
{

    av_log(NULL, AV_LOG_DEBUG,
           "mpegts_add_stream: at pid 0x%x with type %i\n", item->pid, item->type);

    if (ts->pid_cnt < PMT_PIDS_MAX)
    {
        if (item->type == STREAM_TYPE_DSMCC_B)
        {
            SectionContext *sect = NULL;
            AVStream *st = NULL;
            sect = add_section_stream(ts, item->pid, item->type);
            if (!sect)
            {
                av_log(NULL, AV_LOG_ERROR, "mpegts_add_stream: "
                       "error creating Section context for pid 0x%x with type %i\n",
                       item->pid, item->type);
                return;
            }

            st = new_section_av_stream(sect, 0);
            if (!st)
            {
                av_log(NULL, AV_LOG_ERROR, "mpegts_add_stream: "
                       "error creating A/V stream for pid 0x%x with type %i\n",
                       item->pid, item->type);
                return;
            }

            st->component_tag = item->dvbci.component_tag;
            st->codec->flags = item->dvbci.data_id;
            st->codec->sub_id = item->dvbci.carousel_id;

            ts->pmt_pids[ts->pid_cnt] = item->pid;
            ts->pid_cnt++;

            av_log(NULL, AV_LOG_DEBUG, "mpegts_add_stream: "
                   "stream #%d, has id 0x%x and codec %s, type %s at 0x%x\n",
                   st->index, st->id, codec_id_string(st->codec->codec_id),
                   codec_type_string(st->codec->codec_type), st);
        } else {
            PESContext *pes = NULL;
            AVStream *st = NULL;
            pes = add_pes_stream(ts, item->pid, item->type);
            if (!pes)
            {
                av_log(NULL, AV_LOG_ERROR, "mpegts_add_stream: "
                       "error creating PES context for pid 0x%x with type %i\n",
                       item->pid, item->type);
                return;
            }

            /* Pretend it's audio if we have a language. */
            st = new_pes_av_stream(pes, item->dvbci.language[0] ? 0x1c0 : 0);
            if (!st)
            {
                av_log(NULL, AV_LOG_ERROR, "mpegts_add_stream: "
                       "error creating A/V stream for pid 0x%x with type %i\n",
                       item->pid, item->type);
                return;
            }

            ts->pmt_pids[ts->pid_cnt] = item->pid;
            ts->pid_cnt++;

            if (item->dvbci.language[0])
                memcpy(st->language, item->dvbci.language, sizeof(char) * 4);
 
            if (item->dvbci.sub_id && (item->type == STREAM_TYPE_SUBTITLE_DVB))
                st->codec->sub_id = item->dvbci.sub_id;

            st->component_tag = item->dvbci.component_tag;

            av_log(NULL, AV_LOG_DEBUG, "mpegts_add_stream: "
                   "stream #%d, has id 0x%x and codec %s, type %s at 0x%x\n",
                   st->index, st->id, codec_id_string(st->codec->codec_id),
                   codec_type_string(st->codec->codec_type), st);
        }
    }
    else
    {
        av_log(NULL, AV_LOG_ERROR,
               "ERROR: adding pes stream at pid 0x%x, pid_cnt = %i\n",
               item->pid, ts->pid_cnt);
    }
}

void mpegts_remove_stream(MpegTSContext *ts, int pid)
{
    av_log(NULL, AV_LOG_DEBUG, "mpegts_remove_stream 0x%x\n", pid);
    if (ts->pids[pid])
    {
        av_log(NULL, AV_LOG_DEBUG, "closing filter for pid 0x%x\n", pid);
        mpegts_close_filter(ts, ts->pids[pid]);
    }
    int indx = find_in_list(ts->pmt_pids, pid);
    if (indx >= 0)
    {
        memmove(ts->pmt_pids+indx, ts->pmt_pids+indx+1, PMT_PIDS_MAX-indx-1);
        ts->pmt_pids[PMT_PIDS_MAX-1] = 0;
        ts->pid_cnt--;
    }
    else
    {
        av_log(NULL, AV_LOG_DEBUG, "ERROR: closing filter for pid 0x%x, indx = %i\n", pid, indx);
    }
}

static void pat_cb(void *opaque, const uint8_t *section, int section_len)
{
    MpegTSContext *ts = opaque;
    SectionHeader h1, *h = &h1;
    const uint8_t *p, *p_end;
    char buf[256];

    int pmt_pnums[PAT_MAX_PMT];
    int pmt_pids[PAT_MAX_PMT];
    uint pmt_count = 0;
    int i;

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "PAT:\n");
    av_hex_dump(stdout, (uint8_t *)section, section_len);
#endif
    p_end = section + section_len - 4;
    p = section;
    if (parse_section_header(h, &p, p_end) < 0)
        return;
    if (h->tid != PAT_TID)
        return;

    for (i = 0; i < PAT_MAX_PMT; ++i)
    {
        pmt_pnums[i] = get16(&p, p_end);
        if (pmt_pnums[i] < 0)
            break;

        pmt_pids[i] = get16(&p, p_end) & 0x1fff;
        if (pmt_pids[i] < 0)
            break;

        pmt_count++;

#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG,
               "MPEG Program Number=0x%x pid=0x%x req_sid=0x%x\n",
               pmt_pnums[i], pmt_pids[i], ts->req_sid);
#endif
    }

    if (!is_pat_same(ts, pmt_pnums, pmt_pids, pmt_count))
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "New PAT!\n");
#endif
        /* if there were services, get rid of them */
        if (ts->nb_services)
        {
            for (i = ts->nb_services - 1; i >= 0; --i)
            {
                av_free(ts->services[i]->provider_name);
                av_free(ts->services[i]->name);
                av_free(ts->services[i]);
                ts->services[i] = NULL;
            }
            ts->nb_services = 0;
            ts->services = NULL;
        }

        /* if there are new services, add them */
        for (i = 0; i < pmt_count; ++i)
        {
            snprintf(buf, sizeof(buf), "MPEG Program %x", pmt_pnums[i]);
            new_service(ts, pmt_pnums[i], pmt_pids[i],
                        av_strdup(""), av_strdup(buf));
        }
    }

    int found = 0;
    for (i = 0; i < pmt_count; ++i)
    {
        /* if an MPEG program number is requested, and this is that program,
         * add a filter for the PMT. */
        if (ts->req_sid == pmt_pnums[i])
        {
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG, "Found program number!\n");
#endif
            /* close old filter if it doesn't match */
            if (ts->pmt_filter)
            {
                MpegTSFilter *f = ts->pmt_filter;
                MpegTSSectionFilter *sec = &f->u.section_filter;

                if ((f->pid != pmt_pids[i])     ||
                    (f->type != MPEGTS_SECTION) ||
                    (sec->section_cb != pmt_cb) ||
                    (sec->opaque != ts))
                {
                    mpegts_close_filter(ts, ts->pmt_filter);
                    ts->pmt_filter = NULL;
                }
            }

            /* create new pmt_filter if we need one */
            if (!ts->pmt_filter)
            {
                ts->pmt_filter = mpegts_open_section_filter(
                    ts, pmt_pids[i], pmt_cb, ts, 1);
            }

            found = 1;
        }
    }


    /* if we are scanning for any PAT and not a particular PMT,
     * tell parser it is safe to quit. */
    if (ts->req_sid < 0 && ts->scanning)
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "Found PAT, ending scan\n");
#endif
        ts->stop_parse = 1;
    }

    /* if we are looking for a particular MPEG program number,
     * and it is not in this PAT indicate this in "pmt_scan_state"
     * and tell parser it is safe to quit. */ 
    if (ts->req_sid >= 0 && !found)
    {
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "Program 0x%x is not in PAT, ending scan\n",
               ts->req_sid);
#endif
        ts->pmt_scan_state = PMT_NOT_IN_PAT;
        ts->stop_parse = 1;
    }
}

#if 0
static void pat_scan_cb(void *opaque, const uint8_t *section, int section_len)
{
    MpegTSContext *ts = opaque;
    SectionHeader h1, *h = &h1;
    const uint8_t *p, *p_end;
    int sid, pmt_pid;
    char *provider_name, *name;
    char buf[256];

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "PAT:\n");
    av_hex_dump(stdout, (uint8_t *)section, section_len);
#endif
    p_end = section + section_len - 4;
    p = section;
    if (parse_section_header(h, &p, p_end) < 0)
        return;
    if (h->tid != PAT_TID)
        return;

    for(;;) {
        sid = get16(&p, p_end);
        if (sid < 0)
            break;
        pmt_pid = get16(&p, p_end) & 0x1fff;
        if (pmt_pid < 0)
            break;
#ifdef DEBUG_SI
        av_log(NULL, AV_LOG_DEBUG, "sid=0x%x pid=0x%x\n", sid, pmt_pid);
#endif
        if (sid == 0x0000) {
            /* NIT info */
        } else {
            /* add the service with a dummy name */
            snprintf(buf, sizeof(buf), "Service %x", sid);
            name = av_strdup(buf);
            provider_name = av_strdup("");
            if (name && provider_name) {
                new_service(ts, sid, provider_name, name);
            } else {
                av_freep(&name);
                av_freep(&provider_name);
            }
        }
    }
    ts->stop_parse = 1;

    /* remove filter */
    mpegts_close_filter(ts, ts->pat_filter);
    ts->pat_filter = NULL;
#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "end of scan PAT\n");
#endif    
}
#endif

#if 0
static void sdt_cb(void *opaque, const uint8_t *section, int section_len)
{
    MpegTSContext *ts = opaque;
    SectionHeader h1, *h = &h1;
    const uint8_t *p, *p_end, *desc_list_end, *desc_end;
    int onid, val, sid, desc_list_len, desc_tag, desc_len, service_type;
    char *name, *provider_name;

#ifdef DEBUG_SI
    av_log(NULL, AV_LOG_DEBUG, "SDT:\n");
    av_hex_dump(stdout, (uint8_t *)section, section_len);
#endif

    p_end = section + section_len - 4;
    p = section;
    if (parse_section_header(h, &p, p_end) < 0)
        return;
    if (h->tid != SDT_TID)
        return;
    onid = get16(&p, p_end);
    if (onid < 0)
        return;
    val = get8(&p, p_end);
    if (val < 0)
        return;
    for(;;) {
        sid = get16(&p, p_end);
        if (sid < 0)
            break;
        val = get8(&p, p_end);
        if (val < 0)
            break;
        desc_list_len = get16(&p, p_end) & 0xfff;
        if (desc_list_len < 0)
            break;
        desc_list_end = p + desc_list_len;
        if (desc_list_end > p_end)
            break;
        for(;;) {
            desc_tag = get8(&p, desc_list_end);
            if (desc_tag < 0)
                break;
            desc_len = get8(&p, desc_list_end);
            desc_end = p + desc_len;
            if (desc_end > desc_list_end)
                break;
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG, "tag: 0x%02x len=%d\n", desc_tag, desc_len);
#endif
            switch(desc_tag) {
            case 0x48:
                service_type = get8(&p, p_end);
                if (service_type < 0)
                    break;
                provider_name = getstr8(&p, p_end);
                if (!provider_name)
                    break;
                name = getstr8(&p, p_end);
                if (!name)
                    break;
                new_service(ts, sid, provider_name, name);
                break;
            default:
                break;
            }
            p = desc_end;
        }
        p = desc_list_end;
    }
    ts->stop_parse = 1;

    /* remove filter */
    mpegts_close_filter(ts, ts->sdt_filter);
    ts->sdt_filter = NULL;
}
#endif

#if 0
/* scan services in a transport stream by looking at the SDT */
void mpegts_scan_sdt(MpegTSContext *ts)
{
    ts->sdt_filter = mpegts_open_section_filter(ts, SDT_PID, 
                                                sdt_cb, ts, 1);
}
#endif

#if 0
/* scan services in a transport stream by looking at the PAT (better
   than nothing !) */
void mpegts_scan_pat(MpegTSContext *ts)
{
    ts->pat_filter = mpegts_open_section_filter(ts, PAT_PID, 
                                                pat_scan_cb, ts, 1);
}
#endif

static int64_t get_pts(const uint8_t *p)
{
    int64_t pts;
    int val;

    pts = (int64_t)((p[0] >> 1) & 0x07) << 30;
    val = (p[1] << 8) | p[2];
    pts |= (int64_t)(val >> 1) << 15;
    val = (p[3] << 8) | p[4];
    pts |= (int64_t)(val >> 1);
    return pts;
}


static void init_stream(AVStream *st, int stream_type, int code)
{
    int codec_type=-1, codec_id=-1;
    switch(stream_type) {
        case STREAM_TYPE_AUDIO_MPEG1:
        case STREAM_TYPE_AUDIO_MPEG2:
            codec_type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_MP3;
            break;
        case STREAM_TYPE_VIDEO_MPEG1:
        case STREAM_TYPE_VIDEO_MPEG2:
            codec_type = CODEC_TYPE_VIDEO;
            codec_id = CODEC_ID_MPEG2VIDEO;
            break;
        case STREAM_TYPE_VIDEO_MPEG4:
            codec_type = CODEC_TYPE_VIDEO;
            codec_id = CODEC_ID_MPEG4;
            break;
        case STREAM_TYPE_VIDEO_H264:
            codec_type = CODEC_TYPE_VIDEO;
            codec_id = CODEC_ID_H264;
            break;
        case STREAM_TYPE_AUDIO_AAC:
            codec_type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_AAC;
            break;
        case STREAM_TYPE_AUDIO_AC3:
            codec_type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_AC3;
            break;
        case STREAM_TYPE_AUDIO_DTS:
            codec_type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_DTS;
            break;
        case STREAM_TYPE_VBI_DVB:
            codec_type = CODEC_TYPE_DATA;
            codec_id = CODEC_ID_DVB_VBI;
            break;
        case STREAM_TYPE_SUBTITLE_DVB:
            codec_type = CODEC_TYPE_SUBTITLE;
            codec_id = CODEC_ID_DVB_SUBTITLE;
            break;
        case STREAM_TYPE_DSMCC_B:
            codec_type = CODEC_TYPE_DATA;
            codec_id = CODEC_ID_DSMCC_B;
            break;
        case STREAM_TYPE_PRIVATE_DATA:
        default:
            if (code >= 0x1c0 && code <= 0x1df) {
                codec_type = CODEC_TYPE_AUDIO;
                codec_id = CODEC_ID_MP2;
            } else if (code == 0x1bd) {
                codec_type = CODEC_TYPE_AUDIO;
                codec_id = CODEC_ID_AC3;
            } else {
                codec_type = CODEC_TYPE_VIDEO;
                codec_id = CODEC_ID_MPEG1VIDEO;
            }
            break;
    }
    st->codec->codec_type = codec_type;
    st->codec->codec_id = codec_id;
    av_set_pts_info(st, 33, 1, 90000);
}

static AVStream *new_pes_av_stream(PESContext *pes, uint32_t code)
{
    CHECKED_ALLOCZ(pes->st, sizeof(AVStream));
    pes->st->codec = avcodec_alloc_context();
    init_stream(pes->st, pes->stream_type, code);  /* sets codec type and id */
    pes->st->priv_data = pes;
    pes->st->need_parsing = 1;

    pes->st = av_add_stream(pes->stream, pes->st, pes->pid);
fail: /*for the CHECKED_ALLOCZ macro*/
    return pes->st;
}

static AVStream *new_section_av_stream(SectionContext *sect, uint32_t code)
{
    CHECKED_ALLOCZ(sect->st, sizeof(AVStream));
    sect->st->codec = avcodec_alloc_context();
    init_stream(sect->st, sect->stream_type, code);  /* sets codec type and id */
    sect->st->priv_data = sect;
    sect->st->need_parsing = 1;

    sect->st = av_add_stream(sect->stream, sect->st, sect->pid);
fail: /*for the CHECKED_ALLOCZ macro*/
    return sect->st;
}


/* return non zero if a packet could be constructed */
static void mpegts_push_data(void *opaque,
                             const uint8_t *buf, int buf_size, int is_start,
                             int64_t position)
{
    PESContext *pes = opaque;
    MpegTSContext *ts = pes->ts;
    const uint8_t *p;
    int len, code;
    
    if (is_start) {
        pes->startpos = position;
        pes->state = MPEGTS_HEADER;
        pes->data_index = 0;
    }
    p = buf;
    while (buf_size > 0) {
        switch(pes->state) {
        case MPEGTS_HEADER:
            len = PES_START_SIZE - pes->data_index;
            if (len > buf_size)
                len = buf_size;
            memcpy(pes->header + pes->data_index, p, len);
            pes->data_index += len;
            p += len;
            buf_size -= len;
            if (pes->data_index == PES_START_SIZE) {
                /* we got all the PES or section header. We can now
                   decide */
#if 0
                av_hex_dump(pes->header, pes->data_index);
#endif
                if (pes->header[0] == 0x00 && pes->header[1] == 0x00 &&
                    pes->header[2] == 0x01) {
                    /* it must be an mpeg2 PES stream */
                    code = pes->header[3] | 0x100;
                    if (!((code >= 0x1c0 && code <= 0x1df) ||
                          (code >= 0x1e0 && code <= 0x1ef) ||
                          (code == 0x1bd)))
                        goto skip;
                    if (!pes->st && 0 == new_pes_av_stream(pes, code)) {
                        av_log(NULL, AV_LOG_ERROR, "Error: new_pes_av_stream() "
                               "failed in mpegts_push_data\n");
                        goto skip;
                    }
                    pes->state = MPEGTS_PESHEADER_FILL;
                    pes->total_size = (pes->header[4] << 8) | pes->header[5];
                    /* NOTE: a zero total size means the PES size is
                       unbounded */
                    if (pes->total_size)
                        pes->total_size += 6;
                    pes->pes_header_size = pes->header[8] + 9;
                } else {
                    /* otherwise, it should be a table */
                    /* skip packet */
                skip:
                    pes->state = MPEGTS_SKIP;
                    continue;
                }
            }
            break;
            /**********************************************/
            /* PES packing parsing */
        case MPEGTS_PESHEADER_FILL:
            len = pes->pes_header_size - pes->data_index;
            if (len > buf_size)
                len = buf_size;
            memcpy(pes->header + pes->data_index, p, len);
            pes->data_index += len;
            p += len;
            buf_size -= len;
            if (pes->data_index == pes->pes_header_size) {
                const uint8_t *r;
                unsigned int flags;

                flags = pes->header[7];
                r = pes->header + 9;
                pes->pts = AV_NOPTS_VALUE;
                pes->dts = AV_NOPTS_VALUE;
                if ((flags & 0xc0) == 0x80) {
                    pes->pts = get_pts(r);
                    r += 5;
                } else if ((flags & 0xc0) == 0xc0) {
                    pes->pts = get_pts(r);
                    r += 5;
                    pes->dts = get_pts(r);
                    r += 5;
                }
                /* we got the full header. We parse it and get the payload */
                pes->state = MPEGTS_PAYLOAD;
            }
            break;
        case MPEGTS_PAYLOAD:
            if (pes->total_size) {
                len = pes->total_size - pes->data_index;
                if (len > buf_size)
                    len = buf_size;
            } else {
                len = buf_size;
            }
            if (len > 0) {
                AVPacket *pkt = ts->pkt;
                if (pkt && pes->st && av_new_packet(pkt, len) == 0) {
                    memcpy(pkt->data, p, len);
                    pkt->stream_index = pes->st->index;
                    pkt->pts = pes->pts;
                    pkt->dts = pes->dts;
                    pkt->pos = pes->startpos;
                    /* reset pts values */
                    pes->pts = AV_NOPTS_VALUE;
                    pes->dts = AV_NOPTS_VALUE;
                    ts->stop_parse = 1;
                    return;
                }
            }
            buf_size = 0;
            break;
        case MPEGTS_SKIP:
            buf_size = 0;
            break;
        }
    }
}

static PESContext *add_pes_stream(MpegTSContext *ts, int pid, int stream_type)
{
    MpegTSFilter *tss = ts->pids[pid];
    PESContext *pes = 0;
    if (tss) { /* filter already exists */
        if (tss->type == MPEGTS_PES)
            pes = (PESContext*) tss->u.pes_filter.opaque;
        if (pes && (pes->stream_type == stream_type)) {
            return pes; /* if it's the same stream type, just return ok */
        }
        /* otherwise, kill it, and start a new stream */
        mpegts_close_filter(ts, tss);
    }

    /* create a PES context */
    if (!(pes=av_mallocz(sizeof(PESContext)))) {
        av_log(NULL, AV_LOG_ERROR, "Error: av_mallocz() failed in add_pes_stream");
        return 0;
    }
    pes->ts = ts;
    pes->stream = ts->stream;
    pes->pid = pid;
    pes->stream_type = stream_type;
    tss = mpegts_open_pes_filter(ts, pid, mpegts_push_data, pes);
    if (!tss) {
        av_free(pes);
        av_log(NULL, AV_LOG_ERROR, "Error: unable to open mpegts PES filter in add_pes_stream");
        return 0;
    }

    return pes;
}

/* mpegts_push_section: return one or more tables.  The tables may not completely fill
   the packet and there may be stuffing bytes at the end.
   This is complicated because a single TS packet may result in several tables being
   produced.  We may have a "start" bit indicating, in effect, the end of a table but
   the rest of the TS packet after the start may be filled with one or more small tables.
*/
static void mpegts_push_section(void *opaque, const uint8_t *section, int section_len)
{
    SectionContext *sect = opaque;
    MpegTSContext *ts = sect->ts;
    SectionHeader header;
    AVPacket *pkt = ts->pkt;
    const uint8_t *p = section, *p_end = section + section_len - 4;
    if (parse_section_header(&header, &p, p_end) < 0)
    {
        av_log(NULL, AV_LOG_DEBUG, "Unable to parse header\n");
        return;
    }
    if (pkt->data) { /* We've already added at least one table. */
        uint8_t *data = pkt->data;
        int space = pkt->size;
        int table_size = 0;
        while (space > 3 + table_size) {
            table_size = (((data[1] & 0xf) << 8) | data[2]) + 3;
            if (table_size < space) {
                space -= table_size;
                data += table_size;
            } /* Otherwise we've got filler. */
        }
        if (space < section_len) {
            av_log(NULL, AV_LOG_DEBUG, "Insufficient space for additional packet\n");
            return;
        }
        memcpy(data, section, section_len);
    } else if (pkt && sect->st) {
        int pktLen = section_len + 184; /* Add enough for a complete TS payload. */
        if (av_new_packet(pkt, pktLen) == 0) {
            memcpy(pkt->data, section, section_len);
            memset(pkt->data+section_len, 0xff, pktLen-section_len);
            pkt->stream_index = sect->st->index;
            pkt->pts = AV_NOPTS_VALUE;
            pkt->dts = AV_NOPTS_VALUE;
            pkt->pos = 0;
            ts->stop_parse = 1;
        }
   }
}

static SectionContext *add_section_stream(MpegTSContext *ts, int pid, int stream_type)
{
    MpegTSFilter *tss = ts->pids[pid];
    SectionContext *sect = 0;
    if (tss) { /* filter already exists */
        if (tss->type == MPEGTS_SECTION)
            sect = (SectionContext*) tss->u.section_filter.opaque;

        if (sect && (sect->stream_type == stream_type))
            return sect; /* if it's the same stream type, just return ok */

        /* otherwise, kill it, and start a new stream */
        mpegts_close_filter(ts, tss);
    }

    /* create a SECTION context */
    if (!(sect=av_mallocz(sizeof(SectionContext)))) {
        av_log(NULL, AV_LOG_ERROR, "Error: av_mallocz() failed in add_pes_stream");
        return 0;
    }
    sect->ts = ts;
    sect->stream = ts->stream;
    sect->pid = pid;
    sect->stream_type = stream_type;
    tss = mpegts_open_section_filter(ts, pid, mpegts_push_section, sect, 1);
    if (!tss) {
        av_free(sect);
        av_log(NULL, AV_LOG_ERROR, "Error: unable to open mpegts Section filter in add_section_stream");
        return 0;
    }

    return sect;
}


/* handle one TS packet */
static void handle_packet(MpegTSContext *ts, const uint8_t *packet, int64_t position)
{
    if (!ts->pids[0]) {
        /* make sure we're always scanning for new PAT's */
        ts->pat_filter = mpegts_open_section_filter(ts, PAT_PID, pat_cb, ts, 1);
    }
    AVFormatContext *s = ts->stream;
    MpegTSFilter *tss;
    int len, pid, cc, cc_ok, afc, is_start;
    const uint8_t *p, *p_end;

    pid = ((packet[1] & 0x1f) << 8) | packet[2];
    is_start = packet[1] & 0x40;
    tss = ts->pids[pid];
    if (ts->auto_guess && tss == NULL && is_start) {
        PESContext *pes = add_pes_stream(ts, pid, 0);
        if (pes)
            new_pes_av_stream(pes, 0);

        tss = ts->pids[pid];
    }
    if (!tss)
        return;

    /* continuity check (currently not used) */
    cc = (packet[3] & 0xf);
    cc_ok = (tss->last_cc < 0) || ((((tss->last_cc + 1) & 0x0f) == cc));
    tss->last_cc = cc;
    
    /* skip adaptation field */
    afc = (packet[3] >> 4) & 3;
    p = packet + 4;
    if (afc == 0) /* reserved value */
        return;
    if (afc == 2) /* adaptation field only */
        return;
    if (afc == 3) {
        /* skip adapation field */
        p += p[0] + 1;
    }
    /* if past the end of packet, ignore */
    p_end = packet + TS_PACKET_SIZE;
    if (p >= p_end)
        return;
    
    if (tss->type == MPEGTS_SECTION) {
        if (is_start) {
            /* pointer field present */
            len = *p++;
            if (p + len > p_end)
                return;
            if (len && cc_ok) {
                /* write remaining section bytes */
                write_section_data(s, tss, 
                                   p, len, 0);
                /* check whether filter has been closed */
                if (!ts->pids[pid])
                    return;
            }
            p += len;
            if (p < p_end) {
                write_section_data(s, tss, 
                                   p, p_end - p, 1);
            }
        } else {
            if (cc_ok) {
                write_section_data(s, tss, 
                                   p, p_end - p, 0);
            }
        }
    } else {
        tss->u.pes_filter.pes_cb(tss->u.pes_filter.opaque, 
                                 p, p_end - p, is_start, position);
    }
}

/* XXX: try to find a better synchro over several packets (use
   get_packet_size() ?) */
static int mpegts_resync(ByteIOContext *pb)
{
    int c, i;

    for(i = 0;i < MAX_RESYNC_SIZE; i++) {
        c = url_fgetc(pb);
        if (c < 0)
            return -1;
        if (c == 0x47) {
            url_fseek(pb, -1, SEEK_CUR);
            return 0;
        }
    }
    /* no sync found */
    return -1;
}

/* return -1 if error or EOF. Return 0 if OK. */
static int read_packet(ByteIOContext *pb, uint8_t *buf, int raw_packet_size, int64_t *position)
{
    int skip, len;

    for(;;) {
        *position = url_ftell(pb);
        len = get_buffer(pb, buf, TS_PACKET_SIZE);
        if (len != TS_PACKET_SIZE)
            return AVERROR_IO;
        /* check paquet sync byte */
        if (buf[0] != 0x47) {
            /* find a new packet start */
            url_fseek(pb, -TS_PACKET_SIZE, SEEK_CUR);
            if (mpegts_resync(pb) < 0)
                return AVERROR_INVALIDDATA;
            else
                continue;
        } else {
            skip = raw_packet_size - TS_PACKET_SIZE;
            if (skip > 0)
                url_fskip(pb, skip);
            break;
        }
    }
    return 0;
}

static int handle_packets(MpegTSContext *ts, int nb_packets)
{
    AVFormatContext *s = ts->stream;
    ByteIOContext *pb = &s->pb;
    uint8_t packet[TS_PACKET_SIZE];
    int packet_num, ret;
    int64_t pos;

    ts->stop_parse = 0;
    packet_num = 0;
    for(;;) {
        if (ts->stop_parse)
            break;
        packet_num++;
        if (nb_packets != 0 && packet_num >= nb_packets)
            break;
        ret = read_packet(pb, packet, ts->raw_packet_size, &pos);
        if (ret != 0)
            return ret;
        handle_packet(ts, packet, pos);
    }
    return 0;
}

static int mpegts_probe(AVProbeData *p)
{
#if 1
    const int size= p->buf_size;
    int score, fec_score, dvhs_score;
#define CHECK_COUNT 10

    if (size < (TS_FEC_PACKET_SIZE * CHECK_COUNT))
        return -1;
 
    score    = analyze(p->buf, TS_PACKET_SIZE    *CHECK_COUNT, TS_PACKET_SIZE, NULL);
    dvhs_score = analyze(p->buf, TS_DVHS_PACKET_SIZE    *CHECK_COUNT, TS_DVHS_PACKET_SIZE, NULL); 
    fec_score= analyze(p->buf, TS_FEC_PACKET_SIZE*CHECK_COUNT, TS_FEC_PACKET_SIZE, NULL);
/*av_log(NULL, AV_LOG_DEBUG, "score: %d, fec_score: %d \n", score, fec_score);*/
 
    /* we need a clear definition for the returned score 
       otherwise things will become messy sooner or later */
    if     (score > fec_score && score > dvhs_score && score > 6) return AVPROBE_SCORE_MAX + score     - CHECK_COUNT;
    else if(dvhs_score > score && dvhs_score > fec_score && dvhs_score > 6) return AVPROBE_SCORE_MAX + dvhs_score  - CHECK_COUNT;
    else if(                 fec_score > 6) return AVPROBE_SCORE_MAX + fec_score - CHECK_COUNT;
    else                                    return -1;
#else
    /* only use the extension for safer guess */
    if (match_ext(p->filename, "ts"))
        return AVPROBE_SCORE_MAX;
    else
        return 0;
#endif
}

#if 0
void set_service_cb(void *opaque, int ret)
{
    MpegTSContext *ts = opaque;
    ts->pmt_scan_state = ret;
    ts->stop_parse = 1;
}
#endif

/* return the 90 kHz PCR and the extension for the 27 MHz PCR. return
   (-1) if not available */
static int parse_pcr(int64_t *ppcr_high, int *ppcr_low, 
                     const uint8_t *packet)
{
    int afc, len, flags;
    const uint8_t *p;
    unsigned int v;

    afc = (packet[3] >> 4) & 3;
    if (afc <= 1)
        return -1;
    p = packet + 4;
    len = p[0];
    p++;
    if (len == 0)
        return -1;
    flags = *p++;
    len--;
    if (!(flags & 0x10))
        return -1;
    if (len < 6)
        return -1;
    v = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    *ppcr_high = ((int64_t)v << 1) | (p[4] >> 7);
    *ppcr_low = ((p[4] & 1) << 8) | p[5];
    return 0;
}

static int mpegts_read_header(AVFormatContext *s,
                              AVFormatParameters *ap)
{
    MpegTSContext *ts = s->priv_data;
    ByteIOContext *pb = &s->pb;
    uint8_t buf[1024];
    int len, sid, i;
    int64_t pos;
    MpegTSService *service;
    
    if (ap) {
        ts->mpeg2ts_raw = ap->mpeg2ts_raw;
        ts->mpeg2ts_compute_pcr = ap->mpeg2ts_compute_pcr;
    }

    /* read the first 1024 bytes to get packet size */
    pos = url_ftell(pb);
    len = get_buffer(pb, buf, sizeof(buf));
    if (len != sizeof(buf)) {
        av_log(NULL, AV_LOG_ERROR, "mpegts_read_header: unable to read first 1024 bytes\n");
        goto fail;
    }
    ts->raw_packet_size = get_packet_size(buf, sizeof(buf));
    if (ts->raw_packet_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "mpegts_read_header: packet size incorrect\n");
        goto fail;
    }
    ts->stream = s;
    ts->auto_guess = 0;

    if (!ts->mpeg2ts_raw) {
        /* normal demux */

        if (!ts->auto_guess) {
            /* SDT Scan Removed here. It caused startup delays in TS files
               SDT will not exist in a stripped TS file created by myth. */

            /* we don't want any PMT pid filters created on first pass */
            ts->req_sid = -1;

            ts->scanning = 1;
            ts->pat_filter = mpegts_open_section_filter(
                ts, PAT_PID, pat_cb, ts, 1);
            url_fseek(pb, pos, SEEK_SET);
            handle_packets(ts, MAX_SCAN_PACKETS);
            ts->scanning = 0;

            if (ts->nb_services <= 0) {
               /* Guess this is a raw transport stream with no PAT tables. */
               ts->auto_guess = 1;
               s->ctx_flags |= AVFMTCTX_NOHEADER;
               goto do_pcr;
            }
           
            ts->scanning = 1;
            ts->pmt_scan_state = PMT_NOT_YET_FOUND;
            /* tune to first service found */
            for (i = 0; ((i < ts->nb_services) &&
                         (ts->pmt_scan_state == PMT_NOT_YET_FOUND)); i++)
            {
                service = ts->services[i];
#ifdef DEBUG_SI
                av_log(NULL, AV_LOG_DEBUG, "Tuning to '%s' pnum: 0x%x\n",
                       service->name, service->sid);
#endif
            
                /* now find the info for the first service if we found any,
                otherwise try to filter all PATs */
            
                url_fseek(pb, pos, SEEK_SET);
                ts->req_sid = sid = service->sid;
                handle_packets(ts, MAX_SCAN_PACKETS);

                /* fallback code to deal with broken streams from
                 * DBOX2/Firewire cable boxes. */
                if (ts->pmt_filter && 
                    (ts->pmt_scan_state == PMT_NOT_YET_FOUND))
                {
                    av_log(NULL, AV_LOG_ERROR,
                           "Tuning to '%s' pnum: 0x%x "
                           "without CRC check on PMT\n",
                           service->name, service->sid);
                    /* turn off crc checking */
                    ts->pmt_filter->u.section_filter.check_crc = 0;
                    /* try again */
                    url_fseek(pb, pos, SEEK_SET);
                    ts->req_sid = sid = service->sid;
                    handle_packets(ts, MAX_SCAN_PACKETS);
                }
            }
            ts->scanning = 0;

            /* if we could not find any PMTs, fail */
            if (ts->pmt_scan_state == PMT_NOT_YET_FOUND)
            {
                av_log(NULL, AV_LOG_ERROR,
                       "mpegts_read_header: could not find any PMT's\n");
                goto fail;
            } 
            
#ifdef DEBUG_SI
            av_log(NULL, AV_LOG_DEBUG, "tuning done\n");
#endif
        }
        s->ctx_flags |= AVFMTCTX_NOHEADER;
    } else {
        AVStream *st;
        int pcr_pid, pid, nb_packets, nb_pcrs, ret, pcr_l;
        int64_t pcrs[2], pcr_h, position;
        int packet_count[2];
        uint8_t packet[TS_PACKET_SIZE];
        
        /* only read packets */
    do_pcr: 
        st = av_new_stream(s, 0);
        if (!st) {
            av_log(NULL, AV_LOG_ERROR, "mpegts_read_header: av_new_stream() failed\n");
            goto fail;
        }
        av_set_pts_info(st, 33, 1, 27000000);
        st->codec->codec_type = CODEC_TYPE_DATA;
        st->codec->codec_id = CODEC_ID_MPEG2TS;
        
        /* we iterate until we find two PCRs to estimate the bitrate */
        pcr_pid = -1;
        nb_pcrs = 0;
        nb_packets = 0;
        for(;;) {
            ret = read_packet(&s->pb, packet, ts->raw_packet_size, &position);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "mpegts_read_header: read_packet() failed\n");
                goto fail;
            }
            pid = ((packet[1] & 0x1f) << 8) | packet[2];
            if ((pcr_pid == -1 || pcr_pid == pid) &&
                parse_pcr(&pcr_h, &pcr_l, packet) == 0) {
                pcr_pid = pid;
                packet_count[nb_pcrs] = nb_packets;
                pcrs[nb_pcrs] = pcr_h * 300 + pcr_l;
                nb_pcrs++;
                if (nb_pcrs >= 2)
                    break;
            }
            nb_packets++;
        }
        ts->pcr_pid = pcr_pid;

        /* NOTE1: the bitrate is computed without the FEC */
        /* NOTE2: it is only the bitrate of the start of the stream */
        ts->pcr_incr = (pcrs[1] - pcrs[0]) / (packet_count[1] - packet_count[0]);
        ts->cur_pcr = pcrs[0] - ts->pcr_incr * packet_count[0];
        s->bit_rate = (TS_PACKET_SIZE * 8) * 27e6 / ts->pcr_incr;
        st->codec->bit_rate = s->bit_rate;
        st->start_time = ts->cur_pcr;
#if 0
        av_log(NULL, AV_LOG_DEBUG, "start=%0.3f pcr=%0.3f incr=%d\n",
               st->start_time / 1000000.0, pcrs[0] / 27e6, ts->pcr_incr);
#endif
    }

    url_fseek(pb, pos, SEEK_SET);
    return 0;
 fail:
    return -1;
}

#define MAX_PACKET_READAHEAD ((128 * 1024) / 188)

static int mpegts_raw_read_packet(AVFormatContext *s,
                                  AVPacket *pkt)
{
    MpegTSContext *ts = s->priv_data;
    int ret, i;
    int64_t pcr_h, next_pcr_h, pos, position;
    int pcr_l, next_pcr_l;
    uint8_t pcr_buf[12];

    if (av_new_packet(pkt, TS_PACKET_SIZE) < 0)
        return -ENOMEM;
    ret = read_packet(&s->pb, pkt->data, ts->raw_packet_size, &position);
    if (ret < 0) {
        av_free_packet(pkt);
        return ret;
    }
    if (ts->mpeg2ts_compute_pcr) {
        /* compute exact PCR for each packet */
        if (parse_pcr(&pcr_h, &pcr_l, pkt->data) == 0) {
            /* we read the next PCR (XXX: optimize it by using a bigger buffer */
            pos = url_ftell(&s->pb);
            for(i = 0; i < MAX_PACKET_READAHEAD; i++) {
                url_fseek(&s->pb, pos + i * ts->raw_packet_size, SEEK_SET);
                get_buffer(&s->pb, pcr_buf, 12);
                if (parse_pcr(&next_pcr_h, &next_pcr_l, pcr_buf) == 0) {
                    /* XXX: not precise enough */
                    ts->pcr_incr = ((next_pcr_h - pcr_h) * 300 + (next_pcr_l - pcr_l)) / 
                        (i + 1);
                    break;
                }
            }
            url_fseek(&s->pb, pos, SEEK_SET);
            /* no next PCR found: we use previous increment */
            ts->cur_pcr = pcr_h * 300 + pcr_l;
        }
        pkt->pts = ts->cur_pcr;
        pkt->duration = ts->pcr_incr;
        ts->cur_pcr += ts->pcr_incr;
    }
    pkt->stream_index = 0;
    return 0;
}

static int mpegts_read_packet(AVFormatContext *s,
                              AVPacket *pkt)
{
    MpegTSContext *ts = s->priv_data;

    if (!ts->mpeg2ts_raw) {
        ts->pkt = pkt;
        return handle_packets(ts, 0);
    } else {
        return mpegts_raw_read_packet(s, pkt);
    }
}

static int mpegts_read_close(AVFormatContext *s)
{
    MpegTSContext *ts = s->priv_data;
    int i;
    for(i=0;i<NB_PID_MAX;i++)
        if (ts->pids[i]) mpegts_close_filter(ts, ts->pids[i]);
    return 0;
}

static int64_t mpegts_get_pcr(AVFormatContext *s, int stream_index, 
                              int64_t *ppos, int64_t pos_limit)
{
    MpegTSContext *ts = s->priv_data;
    int64_t pos, timestamp;
    uint8_t buf[TS_PACKET_SIZE];
    int pcr_l, pid;

    const int find_next= 1;
    pos = ((*ppos  + ts->raw_packet_size - 1) / ts->raw_packet_size) * ts->raw_packet_size;
    if (find_next) {
        for(;;) {
            url_fseek(&s->pb, pos, SEEK_SET);
            if (get_buffer(&s->pb, buf, TS_PACKET_SIZE) != TS_PACKET_SIZE)
                return AV_NOPTS_VALUE;
            pid = ((buf[1] & 0x1f) << 8) | buf[2];
            if (pid == ts->pcr_pid &&
                parse_pcr(&timestamp, &pcr_l, buf) == 0) {
                break;
            }
            pos += ts->raw_packet_size;
        }
    } else {
        for(;;) {
            pos -= ts->raw_packet_size;
            if (pos < 0)
                return AV_NOPTS_VALUE;
            url_fseek(&s->pb, pos, SEEK_SET);
            if (get_buffer(&s->pb, buf, TS_PACKET_SIZE) != TS_PACKET_SIZE)
                return AV_NOPTS_VALUE;
            pid = ((buf[1] & 0x1f) << 8) | buf[2];
            if (pid == ts->pcr_pid &&
                parse_pcr(&timestamp, &pcr_l, buf) == 0) {
                break;
            }
        }
    }
    *ppos = pos;
    return timestamp;
}

static int read_seek(AVFormatContext *s, int stream_index, int64_t target_ts, int flags){
    MpegTSContext *ts = s->priv_data;
    uint8_t buf[TS_PACKET_SIZE];
    int64_t pos;

    if(av_seek_frame_binary(s, stream_index, target_ts, flags) < 0)
        return -1;

    pos= url_ftell(&s->pb);

    for(;;) {
        url_fseek(&s->pb, pos, SEEK_SET);
        if (get_buffer(&s->pb, buf, TS_PACKET_SIZE) != TS_PACKET_SIZE)
            return -1;
        if(buf[1] & 0x40) break;
        pos += ts->raw_packet_size;
    }
    url_fseek(&s->pb, pos, SEEK_SET);

    return 0;
}

/**************************************************************/
/* parsing functions - called from other demuxers such as RTP */

MpegTSContext *mpegts_parse_open(AVFormatContext *s)
{
    MpegTSContext *ts;
    
    ts = av_mallocz(sizeof(MpegTSContext));
    if (!ts)
        return NULL;
    /* no stream case, currently used by RTP */
    ts->raw_packet_size = TS_PACKET_SIZE;
    ts->stream = s;
    ts->auto_guess = 1;
    return ts;
}

/* return the consumed length if a packet was output, or -1 if no
   packet is output */
int mpegts_parse_packet(MpegTSContext *ts, AVPacket *pkt,
                        const uint8_t *buf, int len)
{
    int len1;
    int64_t position = 0;

    len1 = len;
    ts->pkt = pkt;
    ts->stop_parse = 0;
    for(;;) {
        if (ts->stop_parse)
            break;
        if (len < TS_PACKET_SIZE)
            return -1;
        if (buf[0] != 0x47) {
            buf++;
            len--;
        } else {
            handle_packet(ts, buf, position);
            buf += TS_PACKET_SIZE;
            len -= TS_PACKET_SIZE;
        }
    }
    return len1 - len;
}

void mpegts_parse_close(MpegTSContext *ts)
{
    int i;

    for(i=0;i<NB_PID_MAX;i++)
        av_free(ts->pids[i]);
    av_free(ts);
}

AVInputFormat mpegts_demux = {
    "mpegts",
    "MPEG2 transport stream format",
    sizeof(MpegTSContext),
    mpegts_probe,
    mpegts_read_header,
    mpegts_read_packet,
    mpegts_read_close,
    read_seek,
    mpegts_get_pcr,
    .flags = AVFMT_SHOW_IDS,
};

int mpegts_init(void)
{
    av_register_input_format(&mpegts_demux);
#ifdef CONFIG_MUXERS
    av_register_output_format(&mpegts_mux);
#endif
    return 0;
}
