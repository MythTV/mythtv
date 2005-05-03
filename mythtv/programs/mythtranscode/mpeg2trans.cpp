#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cstdarg>
#include <cstring>
#include <getopt.h>

#include <iostream>
using namespace std;

#include "mpeg2trans.h"

class GetBits
{
  public:
    GetBits(uint8_t *data) { data_ptr = data; pos = 0; }
    uint32_t GetNext(int bits);
    void SkipNext(int bits) { pos += bits; }
    void MarkerBit(void);
  private:
    uint8_t *data_ptr;
    int pos;
};

class SetBits
{
  public:
    SetBits(uint8_t *data) { data_ptr = data; pos = 0; }
    void SetNext(uint32_t value, int bits);
    void SkipNext(int bits) { pos += bits; }
  private:
    uint8_t *data_ptr;
    int pos;
};

#define get_pos(file_buf) ((file_buf)->pos - ((file_buf)->last_byte - (file_buf)->blkptr))
#define READ_PTS ((GETBITS(0, 3)<<30) | (marker_bit() & 0) | (GETBITS(0, 15) << 15) | \
                  (marker_bit() & 0) | GETBITS(0, 15) | (marker_bit() & 0))
#define READ_PTS_EXT (GETBITS(0, 9) | (marker_bit() & 0))
#define PTS2FLOAT(pts) ((float)(pts).val * (outputContext->time_base.num) / (outputContext->time_base.den))
#define PTS2INT(pts, st) ((int64_t)(pts).val * AV_TIME_BASE * (st->time_base.num) / (st->time_base.den))
#define INT2PTS(val, st) ((val) * (st->time_base.den) / (st->time_base.num) / AV_TIME_BASE)

const int ac3_ratetable[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                         128, 160, 192, 224, 256, 320, 384, 448,
                         512, 576, 640};
const uint8_t ac3_halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};

int mp2_debug = 0;
void DPRINTF(int level, const char *format, ...)
{
  if (level < mp2_debug) {
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
  }
}

void FrameBuffer::setPkt(AVPacket *newpkt, int64_t del)
{
    if (newpkt->size + pkt.size > size)
    {
        pkt.data = (uint8_t *)realloc(pkt.data, pkt.size + newpkt->size);
        size = pkt.size + newpkt->size;
    }
    memcpy(pkt.data + pkt.size, newpkt->data, newpkt->size);
    pkt.size += newpkt->size;
    pkt.pts = newpkt->pts;
    pkt.flags = newpkt->flags;
    delta = del;
}

void FrameBuffer::setPkt(uint8_t *data, uint32_t newsize, int64_t newpts, 
                         int64_t del)
{
    AVPacket newpkt;
    newpkt.size = newsize;
    newpkt.pts = newpts;
    newpkt.data = data;
    setPkt(&newpkt, del);
}

uint32_t GetBits::GetNext(int num)
{
    uint32_t value = 0, gb_long;
    int offset, offset_r, offset_b;

    offset = pos >> 3;
    offset_r = pos & 0x07;
    offset_b = 32 - offset_r;
    gb_long = ntohl(*((uint32_t *) (data_ptr + offset)));
    value = gb_long >> (offset_b - num);
    value = value & (0xffffffff >> (32 - num));
    pos+=num;
    return value;
}

void GetBits::MarkerBit(void)
{
    if (!GetNext(1))
    {
        VERBOSE(VB_IMPORTANT, "Marker bit was not valid!");
        exit(TRANSCODE_BUGGY_EXIT_INVALID_MARKER);
    }
}

void SetBits::SetNext(uint32_t value, int num)
{
    uint32_t sb_long, mask;
    int offset, offset_r, offset_b;

    offset = pos >> 3;
    offset_r = pos & 0x07;
    offset_b = 32 - offset_r;
    mask = ~((uint32_t)((1 << num) -1) << (offset_b - num));
    sb_long = ntohl(*((unsigned long *) (data_ptr + offset)));
    value = value << (offset_b - num);
    sb_long = (sb_long & mask) | value;
    *((unsigned long *)(data_ptr + offset)) = htonl(sb_long);
    pos += num;
}

MPEG2trans::MPEG2trans(QMap<long long, int> *map)
{
    int i;
    video_width = 0;
    video_height = 0;
    video_aspect = 0;
    video_frame_rate = 29970;
    video_bitrate = 0;
    pic_frame_count = 0;

    audio_freq = 48000;
    audio_bitrate = 0;

    goppts_delta = 0;

    fixed_gop_ts = NULL;

    chopping = false;
    chop_end = false;
    skipped_frames = 0;

    got_audio = false;
    got_video = false;
    init_av = false;

    outputContext = NULL;
    inputContext = NULL;

    audioout_st = NULL;
    videoout_st = NULL;

    last_frame_number = 0;
    last_gop_pts = 0;
    chop_start_pts = 0;
    video_frame_state = 0;

    for (i = 0; i < 50; i++)
    {
        audioPool.push(new FrameBuffer);
        videoPool.push(new FrameBuffer);
    }
    if (map)
    {
        QMap<long long, int>::Iterator mapIter;
        for (mapIter = map->begin(); mapIter != map->end(); mapIter++)
        {
            if (mapIter.data())
            {
                long long start;
                start = mapIter.key();
                mapIter++;
                if (mapIter != map->end() && ! mapIter.data())
                    cutlistMap[start] = mapIter.key();
            }
        }
        cutlistIter = cutlistMap.begin();
    }
    use_ac3 = false;
    audioin_index = -1;
    videoin_index = -1;

    av_register_all();
}

MPEG2trans::~MPEG2trans(void) {
};

void MPEG2trans::write_muxed_frame(AVStream *stream, AVPacket *pkt, int advance, int64_t delta)
{
    static int64_t audpts, vidpts;
    if (stream == audioout_st)
    {
        FrameBuffer *buffer = audioPool.pop();
        buffer->setPkt(pkt, delta);
        audioQueue.enqueue(buffer);
        if (audioPool.isEmpty() )
        {
            VERBOSE(VB_IMPORTANT, "Ran out of audio buffers!");
            exit(TRANSCODE_BUGGY_EXIT_NO_AUDIO_BUFFERS);
        }
        DPRINTF(1, "AUDMUX : %d buffers left\n", audioPool.count());
    }
    else
    {
        FrameBuffer *buffer = videoPool.pop();
        buffer->setPkt(pkt, delta);
        if (advance)
            videoQueue.enqueue(buffer);
        else
        {
            videoPool.push(buffer);
            if (videoQueue.isEmpty())
                return;
        }
        if (videoPool.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "Ran out of video buffers!");
            exit(TRANSCODE_BUGGY_EXIT_NO_VIDEO_BUFFERS);
        }
        DPRINTF(1, "VIDMUX : %d buffers left\n", videoPool.count());
    }
    if (! got_audio || ! got_video)
        return;
    if (! init_av)
    {
        if (videoQueue.isEmpty())
            return;
        while (! audioQueue.isEmpty() && 
            audioQueue.head()->getPTS() < videoQueue.head()->getPTS())
        {
            FrameBuffer *buffer = audioQueue.dequeue();
            buffer->Reset();
            audioPool.push(buffer);
        }
        if (audioQueue.isEmpty())
            return;
        init_av = 1;
        init_videoout_stream();
        init_audioout_stream();
        /* set the output parameters (must be done even if no
           parameters). */
        if (av_set_parameters(outputContext, NULL) < 0)
        {
            VERBOSE(VB_IMPORTANT, "Invalid output format parameters");
            exit(TRANSCODE_BUGGY_EXIT_INVALID_OUT_PARAMS);
        }
        /* write the stream header, if any */
        av_write_header(outputContext);
        dump_format(outputContext, 0, outputContext->filename, 1);
        videoout_st->pts.val = INT2PTS(videoQueue.head()->getPTS(), videoout_st);
    }

    while ((vidpts < audpts && ! videoQueue.isEmpty()) ||
          (audpts <= vidpts && ! audioQueue.isEmpty()))
    {
        AVFrame avframe;
        memset(&avframe, 0, sizeof(AVFrame));
        DPRINTF(1, "audpts: %lld vidpts: %lld\n", audpts, vidpts);
        if (audpts <= vidpts) {
            FrameBuffer *buf = audioQueue.dequeue();
            int64_t bufpts = buf->getPTS() - buf->getDelta();
            audioout_st->pts.val = INT2PTS(bufpts, audioout_st);
//            long long orig = PTS2INT(audioout_st->pts, audioout_st);
            audioout_st->codec.coded_frame= &avframe;
            if (av_write_frame(outputContext, buf->getPkt()) != 0)
            {
                VERBOSE(VB_IMPORTANT, "Error while writing audio frame");
                exit(TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR);
            }
            DPRINTF(1, "AUDIO PTS (initial): %lld (stored): %lld\n", buf->getPTS(), PTS2INT(audioout_st->pts, audioout_st));
            audpts += PTS2INT(audioout_st->pts, audioout_st) - bufpts;
            buf->Reset();
            audioPool.push(buf);
        }
        else
        {
            FrameBuffer *buf = videoQueue.dequeue();
            int64_t bufpts = buf->getPTS() - buf->getDelta();
            videoout_st->pts.val = INT2PTS(bufpts, videoout_st);
//            int64_t oldpts = PTS2INT(videoout_st->pts, videoout_st);
            videoout_st->codec.coded_frame= &avframe;
            if (av_write_frame(outputContext, buf->getPkt()) != 0)
            {
                VERBOSE(VB_IMPORTANT, "Error while writing video frame");
                exit(TRANSCODE_BUGGY_EXIT_WRITE_FRAME_ERROR);
            }
            DPRINTF(1, "VIDEO PTS (initial): %lld (stored): %lld\n", buf->getPTS(), PTS2INT(videoout_st->pts, videoout_st));
            vidpts += PTS2INT(videoout_st->pts, videoout_st) - bufpts;
            buf->Reset();
            videoPool.push(buf);
        }
    }
    // empty audio buffer after writing the last video/audio frames
    if (chopping && videoQueue.isEmpty())
    {
        while(! audioQueue.isEmpty())
        {
            FrameBuffer *buf = audioQueue.dequeue();
            buf->Reset();
            audioPool.push(buf);
        }
    }
}

bool MPEG2trans::check_ac3_header(uint8_t *buf)
{
    if ((buf[0] == 0x0b) && (buf[1] == 0x77))
        return true;
    return false;
}
  
bool MPEG2trans::check_mp2_header(uint8_t *buf)
{
    if (buf[0] == 0xff && (buf[1] & 0xf0) == 0xf0)
        return true;
    return false;
}

bool MPEG2trans::check_video_header(uint8_t *buf)
{
    if (buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01)
        return true;
    return false;
}

uint32_t MPEG2trans::process_ac3_audio(AVPacket *pkt)
{
    uint32_t frame_len;
    uint8_t *buf = pkt->data;
    if (check_ac3_header(buf)) {
        uint32_t freq, bitrate, pad, bsid, flags;
        GetBits *gb = new GetBits(pkt->data + 4);
        freq = gb->GetNext(2);
        bitrate = gb->GetNext(5);
        pad = gb->GetNext(1);
        bsid = gb->GetNext(5);
        gb->SkipNext(3); //???
        flags = gb->GetNext(4);
        delete gb;
        if (bitrate < 19 && bsid < 12 && freq != 3) {
            int half;
            half = ac3_halfrate[bsid];
            audio_bitrate = (1000 * ac3_ratetable[bitrate]) >> half;
            switch(freq) {
                case 0 : audio_freq = 48000 >> half; break;
                case 1 : audio_freq = 44100 >> half; break;
                case 2 : audio_freq = 32000 >> half; break;
            }
            frame_len = 192 * audio_bitrate / audio_freq + pad;
            return frame_len;
        }
    }
    return 0;
}

uint32_t MPEG2trans::process_mp2_audio(AVPacket *pkt)
{
    uint32_t frame_len = 0;
    uint8_t *buf = pkt->data;

    if (check_mp2_header(buf))
    {
        uint8_t pad;
        if (buf[1] != 0xfd)
        {
            VERBOSE(VB_IMPORTANT, "Only MP2 audio is currently supported");
            exit(TRANSCODE_BUGGY_EXIT_INVALID_AUDIO);
        }
        GetBits *gb = new GetBits(buf + 2);
        audio_bitrate = gb->GetNext(4);
        audio_freq = gb->GetNext(2);
        pad = gb->GetNext(1);
        delete gb;
        switch (audio_bitrate) {
            case 1:  audio_bitrate = 32000; break;
            case 2:  audio_bitrate = 48000; break;
            case 3:  audio_bitrate = 56000; break;
            case 4:  audio_bitrate = 64000; break;
            case 5:  audio_bitrate = 80000; break;
            case 6:  audio_bitrate = 96000; break;
            case 7:  audio_bitrate = 112000; break;
            case 8:  audio_bitrate = 128000; break;
            case 9:  audio_bitrate = 160000; break;
            case 10: audio_bitrate = 192000; break;
            case 11: audio_bitrate = 224000; break;
            case 12: audio_bitrate = 256000; break;
            case 13: audio_bitrate = 320000; break;
            case 14: audio_bitrate = 384000; break;
            default:
               fprintf(stderr, "Unsupported bitrate 0x%02x\n", audio_bitrate);
               exit(TRANSCODE_BUGGY_EXIT_INVALID_AUDIO);
        }
        switch (audio_freq) {
            case 0: audio_freq = 44100; break;
            case 1: audio_freq = 48000; break;
            case 2: audio_freq = 32000; break;
            default:
                fprintf(stderr, "Unsupported frequency 0x%02x\n", audio_freq);
                exit(TRANSCODE_BUGGY_EXIT_INVALID_AUDIO);
        }
        frame_len = (144 * audio_bitrate / audio_freq) + pad;
        return frame_len;
    }
    return 0;
}

void MPEG2trans::process_audio(AVPacket *pkt)
{
    int32_t frame_len;
    audioFrame.setPkt(pkt);
    AVPacket *thispkt = audioFrame.getPkt();
    if (use_ac3)
        frame_len = process_ac3_audio(thispkt);
    else
        frame_len = process_mp2_audio(thispkt);
    if (frame_len == pkt->size) {
        got_audio = 1;
        bool processed_frame = false;

        if (chopping && thispkt->pts >= chop_start_pts || chop_end)
        {
            while(! audskipQueue.isEmpty() && 
                  audskipQueue.head()->pts < last_gop_pts)
            {
                AVPacket *store = audskipQueue.dequeue();
                delete store;
            }
            if (chopping && thispkt->pts >= last_gop_pts ||
                chop_end && pic_frame_count < 2)
            {
                // store audio frame
                AVPacket *store = new AVPacket;
                store->data = new uint8_t[frame_len];
                memcpy(store->data, thispkt->data, frame_len);
                store->size = frame_len;
                store->pts = thispkt->pts;
                audskipQueue.enqueue(store);
            }
            if (! chop_end || pic_frame_count < 2)
                processed_frame = true;
        }
        if (! processed_frame)
        {
            while(! audskipQueue.isEmpty())
            {
                AVPacket *store = audskipQueue.dequeue();
                write_muxed_frame(audioout_st, store, true, goppts_delta);
                delete [] store->data;
                delete store;
            }
            write_muxed_frame(audioout_st, thispkt, true, goppts_delta);
        }
        DPRINTF(1, "Audio frame: %d bytes freq: %d bitrate: %d ptsdel: %lld\n", 
                   frame_len, audio_freq, audio_bitrate, goppts_delta);
        if (thispkt->size != frame_len)
        {
            memmove(thispkt->data, thispkt->data + frame_len, (thispkt->size - frame_len));
            thispkt->size -= frame_len;
        }
        else
        {
            thispkt->size = 0;
            return;
        }
    }
    uint8_t *frame_start = thispkt->data;
    while (thispkt->size &&
         (use_ac3 && !check_ac3_header(frame_start) ||
          ! use_ac3 && check_mp2_header(frame_start)))
    {
        frame_start++;
        thispkt->size--;
    }
    if (thispkt->size) {
        memmove(thispkt->data, frame_start, thispkt->size);
        DPRINTF(1, "Found %d bytes of garbage data before header\n", 
                   frame_start - thispkt->data);
    } else {
        DPRINTF(1, "Found %d bytes of garbage data, but no header\n",
                   frame_start - thispkt->data);
    }
}

bool MPEG2trans::process_video(AVPacket *pkt, bool gopsearch)
{
    bool skip_pic_frame = false;
    bool found_gop = false;
    uint8_t *vidblkptr, *frame_start = NULL;
    uint32_t len;

    videoFrame.Reset();
    videoFrame.setPkt(pkt);
    AVPacket *thispkt = videoFrame.getPkt();

    vidblkptr = thispkt->data;
    len = thispkt->size;

    // a pkt can contain SEQ and GOP frames, but it is gauranteed that
    // there is one and only one PIC frame in the pkt, and that will be
    // the last frame in the pkt.
    while (len >= 4 && check_video_header(vidblkptr))
    {
        uint32_t frame_len = 0;
        uint32_t pos = vidblkptr - thispkt->data;
        if (vidblkptr[3] == 0xb3) // sequence header
        {
            if (gopsearch)
                return true;
            if (video_width == 0)
            {
                frame_len = 4 + parse_seq_header(vidblkptr + 4, true);
                video_frame_state |= 0x01;
                got_video = 1;
            }
            else
                frame_len = 4 + parse_seq_header(vidblkptr + 4, false);
            if (! chopping && cutlistIter != cutlistMap.end() && 
               last_frame_number >= cutlistIter.key())
            {
                chopping = 1;
                chop_start_pts = thispkt->pts;
            }
            seqFrame.Reset();
            seqFrame.setPkt(vidblkptr, frame_len, thispkt->pts);
            DPRINTF(1, "pos: 0x%lx SEQ-HEADER: w: %d "
                       "h: %d a/r: %d rate: %d br: %d\n",
                       pos, video_width, video_height, video_aspect,
                       video_frame_rate, video_bitrate);
        }
        else if (vidblkptr[3] == 0xb8)
        {
            if (gopsearch)
                return true;
            gop_timestamp_t gop_ts;
            bool closed, broken;
            if (! (video_frame_state | 0x01))
                continue;
            video_frame_state |= 0x02;
            found_gop = true;
            last_gop_pts = thispkt->pts;
            frame_start = vidblkptr;
            if (chop_end)
                chop_end = false;
            last_frame_number += pic_frame_count;
            if (! chopping && cutlistIter != cutlistMap.end() && 
               last_frame_number >= cutlistIter.key() && 
               last_frame_number < cutlistIter.data())
            {
                chopping = 1;
                chop_start_pts = thispkt->pts;
            }
            else if (chopping && cutlistIter != cutlistMap.end() && 
               last_frame_number >= cutlistIter.data())
            {
                chopping = 0;
                chop_end = true;
                cutlistIter++;
            }
            frame_len = 4+ get_gop_data(vidblkptr + 4, &gop_ts, 
                                        &closed, &broken);
            if (fixed_gop_ts)
            {
                if (! chopping)
                    update_timestamp(fixed_gop_ts, &gop_ts);
            }
            else
            {
                fixed_gop_ts = new gop_timestamp_t;
                *fixed_gop_ts = gop_ts;
            }
            int64_t goppts;
            goppts = (fixed_gop_ts->hour * 3600 +
                      fixed_gop_ts->min * 60 +
                      fixed_gop_ts->sec) * AV_TIME_BASE +
                     fixed_gop_ts->pic * AV_TIME_BASE / video_frame_rate;
            goppts_delta = (gop_ts.hour * 3600 + gop_ts.min * 60 + 
                            gop_ts.sec ) * AV_TIME_BASE +
                           gop_ts.pic * AV_TIME_BASE / video_frame_rate - 
                           goppts;
            if (chop_end)
            {
                closed = 0;
                broken = 1;
            }
            set_gop_data(vidblkptr + 4, fixed_gop_ts, closed, broken);
            pic_frame_count = 0;
            skipped_frames = 0;
            DPRINTF(1, "pos: 0x%lx GOP-HEADER %02d:%02d:%02d "
                       "#: %d c: %d b: %d\n", pos, fixed_gop_ts->hour, 
                       fixed_gop_ts->min, fixed_gop_ts->sec,
                       fixed_gop_ts->pic, closed, broken);
        }
        else if (vidblkptr[3] == 0x00)
        {
            if (gopsearch)
                return false;
            int num, type;
            char pictype;
            frame_len = len;
            if (video_frame_state != 0x03)
                continue;
            if (! frame_start)
                frame_start = vidblkptr;
            GetBits *gb = new GetBits(vidblkptr + 4);
            num = gb->GetNext(10);
            type = gb->GetNext(3);
            delete gb;
            switch (type) {
                case 1: pictype = 'I'; break;
                case 2: pictype = 'P'; break;
                case 3: pictype = 'B'; break;
                case 4: pictype = 'D'; break;
                default: pictype = 'X'; break;
            }
            if (0 && chop_end) {
                if (pic_frame_count < 2 && pictype == 'B') {
                    skip_pic_frame = 1;
                    skipped_frames++;
                } else {
                    if (pic_frame_count == 0)
                        num = 0;
                    else
                        num -= skipped_frames;
                    update_pic_frame_num(vidblkptr + 4,num);
                    pic_frame_count++;
                }
            }
            else
                pic_frame_count++;
            DPRINTF(1, "pos: 0x%lx PIC-HEADER %c-Frame %s#: %d\n", pos,
                       pictype, ((skip_pic_frame)? "SKIPPED ":""), num);
            if (pictype == 'X') DPRINTF(1, "Frame type: %d\n", type);
        }
        else if (vidblkptr[3] == 0xbe)
        {
            uint32_t skip = (vidblkptr[4] << 8) + vidblkptr[5];
            frame_len = skip + 6;
            DPRINTF(1, "pos: 0x%lx PAD-HEADER (%d bytes)\n", pos, skip);
        }
        vidblkptr += frame_len;
        len -= frame_len;
    }
    if (gopsearch)
        return false;
    if ((uint32_t)(vidblkptr - thispkt->data) == videoFrame.getSize())
    {
        if (! chopping && !skip_pic_frame && frame_start)
        {
            if (found_gop)
                write_muxed_frame(videoout_st, seqFrame.getPkt(), false);
            AVPacket picpkt;
            picpkt.pts = thispkt->pts;
            picpkt.data = frame_start;
            picpkt.size = thispkt->size - (frame_start - thispkt->data);
            write_muxed_frame(videoout_st, &picpkt, true, goppts_delta);
        }
    }
    else // something went terribly wrong!
    {
        exit(TRANSCODE_BUGGY_EXIT_INVALID_VIDEO);
    }
    return(false);
}

int MPEG2trans::DoTranscode(QString inputFilename, QString outputFilename)
{
    AVPacket pkt1, *pkt = &pkt1;
    AVFormatParameters params, *ap = &params;
    int i, err;

    inputContext = NULL; //av_mallocz(sizeof(AVFormatContext));
    ap = (AVFormatParameters*) calloc(sizeof(AVFormatParameters), 1);
    err = av_open_input_file(&inputContext, inputFilename, NULL, 0, ap);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open file '%1', error code %2")
                .arg(inputFilename).arg(err));
        return 1;
    }
    err = av_find_stream_info(inputContext);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Could not find stream paramters for '%1',"
                        " error code %2").arg(inputFilename).arg(err));
        return 2;
    }
    for (i = 0; i < inputContext->nb_streams; i++)
    {
        AVCodecContext *enc = &inputContext->streams[i]->codec;
        VERBOSE(VB_GENERAL, QString("Stream: %1 Type: %2")
                .arg(i).arg(enc->codec_type));
        switch(enc->codec_type) {
            case CODEC_TYPE_AUDIO:
                if (!use_ac3 && 
                    (inputContext->streams[i])->codec.codec_id == CODEC_ID_AC3) {
                    use_ac3 = 1;
                    audioin_index = i;
                } else if (audioin_index == -1) 
                    audioin_index = i;
                break;
            case CODEC_TYPE_VIDEO:
                videoin_index = i;
                break;
            default:
                break;
        }
    }
    dump_format(inputContext, 0, inputFilename, 0);

        AVOutputFormat *fmt;
        /* initialize libavcodec, and register all codecs and formats */
    
        /* auto detect the output format from the name. default is
           mpeg. */
        fmt = guess_format("vob", NULL, NULL);
        if (!fmt)
        {
            VERBOSE(VB_IMPORTANT, "Could not find suitable output format.");
            return 3;
        }
   
        /* allocate the output media context */
        outputContext = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
        if (!outputContext)
        {
            VERBOSE(VB_IMPORTANT, "Could not allocate memory.");
            return 4;
        }
        outputContext->oformat = fmt;
        snprintf(outputContext->filename,
                 sizeof(outputContext->filename), "%s", outputFilename.ascii());

        /* add the audio and video streams using the default format codecs
           and initialize the codecs */
        videoout_st = av_new_stream(outputContext, 0);
        audioout_st = av_new_stream(outputContext, 1);

        if (!videoout_st || !audioout_st)
        {
            VERBOSE(VB_IMPORTANT,
                    "Could not find relevant audio and video streams!");
            return 5;
        }

        /* open the output file, if needed */
        if (url_fopen(&outputContext->pb, outputFilename, URL_WRONLY) < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Could not open output file '%1'")
                    .arg(outputFilename));
            return 6;
        }

    while (av_read_frame(inputContext, pkt) >= 0)
    {
        DPRINTF(1, "PKT (%s): %lu %llu\n",((pkt->stream_index == audioin_index) ? "AUDIO" : "VIDEO"),
                pkt->size, pkt->pts);
        if (pkt->stream_index == audioin_index)
            process_audio(pkt);
        else if (pkt->stream_index == videoin_index)
            process_video(pkt);
    }
    av_close_input_file(inputContext);

    /* write the trailer, if any */
    av_write_trailer(outputContext);
    
    /* free the streams */
    while (outputContext->nb_streams)
    {
        av_freep(&outputContext->streams[--outputContext->nb_streams]);
    }

    /* close the output file */
    url_fclose(&outputContext->pb);

    /* free the stream */
    av_free(outputContext);
    return 0;
}

uint32_t MPEG2trans::parse_seq_header(uint8_t *PES_data, bool store)
{
    uint32_t len = 0;
    GetBits *gb = new GetBits(PES_data);
    if (store)
    {
        int rate;
        video_width  = gb->GetNext(12);
        video_height = gb->GetNext(12);
        video_aspect = gb->GetNext(4);
        rate = gb->GetNext(4);
        video_bitrate = gb->GetNext(18) * 400;
        switch (rate) {
            case 1: video_frame_rate = 23976; break;
            case 2: video_frame_rate = 24000; break;
            case 3: video_frame_rate = 25000; break;
            case 4: video_frame_rate = 29970; break;
            case 5: video_frame_rate = 30000; break;
            case 6: video_frame_rate = 50000; break;
            case 7: video_frame_rate = 59940; break;
            case 8: video_frame_rate = 60000; break;
        }
    }
    else
    {
        gb->SkipNext(50);
    }
    gb->SkipNext(12);
    if (gb->GetNext(1))
    {
        gb->SkipNext(8*64);
        len += 64;
    }
    if (gb->GetNext(1))
    {
        gb->SkipNext(8*64);
        len += 64;
    }
    delete gb;
    len += 8;
    len += get_ext_frame(PES_data + len);
    return len;
}

// Must be careful using get_ext_frame. There is no bounds checking
// This is ok, because a SEQ or GOP frame must contain a pic frame
// in the same pkt.  get_ext_frame should only be used when it is
// gauranteed that there is another video frame in the packet!
uint32_t MPEG2trans::get_ext_frame(uint8_t *PES_data)
{
    uint32_t len = 0;
    bool done = false;
    while (! done)
    {
        if (! check_video_header(PES_data))
        {
            len++;
            PES_data++;
        }
        else if (PES_data[3] == 0xb2 || PES_data[3] == 0xb5)
        {
            len += 4;
            PES_data +=4;
        }
        else
            done = true;
    }
    return len;
}

uint32_t MPEG2trans::get_gop_data(uint8_t *PES_data, gop_timestamp_t *ts,
                                  bool *closed, bool *broken)
{
    uint32_t len = 4;
    GetBits *gb = new GetBits(PES_data);
    ts->drop = gb->GetNext(1);
    ts->hour = gb->GetNext(5);
    ts->min = gb->GetNext(6);
    gb->MarkerBit();
    ts->sec = gb->GetNext(6);
    ts->pic = gb->GetNext(6);
    *closed = gb->GetNext(1);
    *broken = gb->GetNext(1);
    gb->SkipNext(5);
    delete gb;
    len += get_ext_frame(PES_data + len);
    return len;
}

void MPEG2trans::set_gop_data(uint8_t *PES_data, gop_timestamp_t *ts, 
                              bool closed, bool broken)
{
    SetBits *sb = new SetBits(PES_data);
    sb->SetNext(ts->drop, 1);
    sb->SetNext(ts->hour, 5);
    sb->SetNext(ts->min, 6);
    sb->SetNext(1, 1);
    sb->SetNext(ts->sec, 6);
    sb->SetNext(ts->pic, 6);
    sb->SetNext(closed, 1);
    sb->SetNext(broken, 1);
    delete sb;
}

void MPEG2trans::update_timestamp(gop_timestamp_t *last_ts, gop_timestamp_t *ts)
{
    uint32_t framerate;

    framerate = (video_frame_rate + 500) / 1000;
    last_ts->pic += pic_frame_count;
    while (last_ts->pic >= framerate) {
      last_ts->pic -= framerate;
      last_ts->sec += 1;
    }
    while (last_ts->sec >= 60) {
      last_ts->sec -=60;
      last_ts->min +=1;
    }
    while (last_ts->min >= 60) {
      last_ts->min -=60;
      last_ts->hour +=1;
    }
    DPRINTF(1, "Computed time: %02d:%02d:%02d.%02d "
               "Actual time: %02d:%02d:%02d.%02d %d\n",
               last_ts->hour, last_ts->min, last_ts->sec, last_ts->pic,    
               ts->hour, ts->min, ts->sec, ts->pic, pic_frame_count);
}

void MPEG2trans::update_pic_frame_num(uint8_t *PES_data, int num)
{
      uint8_t val[2];
      val[0] = (num >> 2) && 0xff;
      val[1] = (num & 0x03) << 6;
      val[1] |= (PES_data[1] & 0x3f);
      PES_data[0] = val[0];
      PES_data[1] = val[1];
}

/**************************************************************/
/* audio output */
void MPEG2trans::init_audioout_stream(void)
{
    AVCodecContext *c;

    c = &audioout_st->codec;
    c->codec_id = (inputContext->streams[audioin_index])->codec.codec_id;
    c->codec_type = CODEC_TYPE_AUDIO;

    /* put sample parameters */
    c->bit_rate = audio_bitrate;
    c->sample_rate = audio_freq;
    c->channels = 2;
    DPRINTF(1, "AUDIO: bitrate: %d freq: %d\n", c->bit_rate, c->sample_rate);
}

/**************************************************************/
/* video output */
void MPEG2trans::init_videoout_stream()
{
    AVCodecContext *c;

    c = &videoout_st->codec;
    c->codec_id = (inputContext->streams[videoin_index])->codec.codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = video_bitrate;
    /* resolution must be a multiple of two */
    c->width = video_width;
    c->height = video_height;
    /* frames per second */
    c->frame_rate = video_frame_rate;
    c->frame_rate_base = 1000;
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    /* just for testing, we also add B frames */
    c->max_b_frames = 2;
    DPRINTF(1, "VIDEO: bitrate: %d size: %dx%d framerate: %f\n", 
               c->bit_rate, c->width, c->height, (float)c->frame_rate / c->frame_rate_base);
}

int MPEG2trans::BuildKeyframeIndex(QString filename, QMap <long long, long long> &posMap)
{
    AVPacket pkt1, *pkt = &pkt1;
    AVFormatParameters params, *ap = &params;
    int i, err;
    uint32_t count = 0;

    inputContext = NULL; //av_mallocz(sizeof(AVFormatContext));
    ap = (AVFormatParameters *)calloc(sizeof(AVFormatParameters), 1);   
    err = av_open_input_file(&inputContext, filename, NULL, 0, ap);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Failed to open file '%1', error code %2")
                .arg(filename).arg(err));
        return 1;
    }
    err = av_find_stream_info(inputContext);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Could not find stream paramters for '%1',"
                        " error code %2").arg(filename).arg(err));
        return 2;
    }

    for (i = 0; i < inputContext->nb_streams; i++)
    {
        AVCodecContext *enc = &inputContext->streams[i]->codec;
        if (enc->codec_type == CODEC_TYPE_VIDEO)
        {
            videoin_index = i;
            break;
        }
    }

    while (av_read_frame(inputContext, pkt) >= 0)
    {
        if (pkt->stream_index == videoin_index)
        {
            if (process_video(pkt, true))
                posMap[count] = pkt->startpos;
            count++;
        }
    }
    av_close_input_file(inputContext);
    return 0;
}
