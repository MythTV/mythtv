#include <iostream>
#include <assert.h>
#include <unistd.h>

using namespace std;

#include "avformatdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcontext.h"

extern "C" {
#ifdef USING_XVMC
#include "libavcodec/xvmc_render.h"
#endif
#include "libavcodec/liba52/a52.h"
}

#define MAX_AC3_FRAME_SIZE 6144

extern pthread_mutex_t avcodeclock;

AvFormatDecoder::AvFormatDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                                 ProgramInfo *pginfo)
               : DecoderBase(parent)
{
    m_db = db;
    m_playbackinfo = pginfo;

    ic = NULL;
    directrendering = false;
    video_last_P_pts = lastapts = lastvpts = 0;
    framesPlayed = 0;
    framesRead = 0;

    audio_check_1st = 2;
    audio_sample_size = 4;
    audio_sampling_rate = 48000;

    hasbframes = false;

    hasFullPositionMap = false;
    recordingHasPositionMap = false;
    positionMapByFrame = false;
    posmapStarted = false;

    keyframedist = -1;

    exitafterdecoded = false;
    ateof = false;
    gopset = false;
    seen_gop = false;
    seq_count = 0;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;

    lastKey = 0;

    memset(&params, 0, sizeof(AVFormatParameters));
    memset(prvpkt, 0, 3);

    do_ac3_passthru = gContext->GetNumSetting("AC3PassThru", false);
}

AvFormatDecoder::~AvFormatDecoder()
{
    if (ic)
    {
        for (int i = 0; i < ic->nb_streams; i++) 
        {
            AVStream *st = ic->streams[i];
            if (st->codec.codec)
                avcodec_close(&st->codec);
        } 

        ic->iformat->flags |= AVFMT_NOFILE;

        av_free(ic->pb.buffer);
        av_close_input_file(ic);
        ic = NULL;
    }
}

void AvFormatDecoder::SeekReset(void)
{
    lastapts = 0;
    lastvpts = 0;
    video_last_P_pts = 0;

    av_read_frame_flush(ic);

    ic->pb.pos = ringBuffer->GetTotalReadPosition();
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        if (enc->codec)
            avcodec_flush_buffers(enc);
    }

    VideoFrame *buffer;
    for (buffer = inUseBuffers.first(); buffer; buffer = inUseBuffers.next())
    {
        m_parent->DiscardVideoFrame(buffer);
    }

    inUseBuffers.clear();

    while (storedPackets.count() > 0)
    {
        AVPacket *pkt = storedPackets.first();
        storedPackets.removeFirst();
        av_free_packet(pkt);
        delete pkt;
    }

    prevgoppos = 0;
    gopset = false;
}

void AvFormatDecoder::Reset(void)
{
    SeekReset();

    m_positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;
    seen_gop = false;
    seq_count = 0;
}

bool AvFormatDecoder::CanHandle(char testbuf[2048], const QString &filename)
{
    av_register_all();

    AVProbeData probe;

    probe.filename = (char *)(filename.ascii());
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = 2048;

    if (av_probe_input_format(&probe, true))
        return true;
    return false;
}

int open_avf(URLContext *h, const char *filename, int flags)
{
    (void)h;
    (void)filename;
    (void)flags;
    return 0;
}

int read_avf(URLContext *h, uint8_t *buf, int buf_size)
{
    AvFormatDecoder *dec = (AvFormatDecoder *)h->priv_data;

    return dec->getRingBuf()->Read(buf, buf_size);
}

int write_avf(URLContext *h, uint8_t *buf, int buf_size)
{
    (void)h;
    (void)buf;
    (void)buf_size;
    return 0;
}

offset_t seek_avf(URLContext *h, offset_t offset, int whence)
{
    AvFormatDecoder *dec = (AvFormatDecoder *)h->priv_data;

    return dec->getRingBuf()->Seek(offset, whence);
}

int close_avf(URLContext *h)
{
    (void)h;
    return 0;
}

URLProtocol rbuffer_protocol = {
    "rbuffer",
    open_avf,
    read_avf,
    write_avf,
    seek_avf,
    close_avf,
    NULL
};

static void avf_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *h = (URLContext *)opaque;
    url_write(h, buf, buf_size);
}

static int avf_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *h = (URLContext *)opaque;
    return url_read(h, buf, buf_size);
}

static int avf_seek_packet(void *opaque, int64_t offset, int whence)
{
    URLContext *h = (URLContext *)opaque;
    url_seek(h, offset, whence);
    return 0;
}

void AvFormatDecoder::InitByteContext(void)
{
    readcontext.prot = &rbuffer_protocol;
    readcontext.flags = 0;
    readcontext.is_streamed = 0;
    readcontext.max_packet_size = 0;
    readcontext.priv_data = this;

    ic->pb.buffer_size = 32768;
    ic->pb.buffer = (unsigned char *)av_malloc(ic->pb.buffer_size);
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.write_flag = 0;
    ic->pb.buf_end = ic->pb.buffer;
    ic->pb.opaque = &readcontext;
    ic->pb.read_packet = avf_read_packet;
    ic->pb.write_packet = avf_write_packet;
    ic->pb.seek = avf_seek_packet;
    ic->pb.pos = 0;
    ic->pb.must_flush = 0;
    ic->pb.eof_reached = 0;
    ic->pb.is_streamed = 0;
    ic->pb.max_packet_size = 0;
}

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

int AvFormatDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                              char testbuf[2048])
{
    ringBuffer = rbuffer;

    AVInputFormat *fmt = NULL;
    char *filename = (char *)(rbuffer->GetFilename().ascii());

    AVProbeData probe;
    probe.filename = filename;
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = 2048;

    fmt = av_probe_input_format(&probe, true);
    if (!fmt)
    {
        cerr << "Couldn't decode file\n";
        return -1;
    }

    fmt->flags |= AVFMT_NOFILE;

    ic = (AVFormatContext *)av_mallocz(sizeof(AVFormatContext));
    if (!ic)
    {
        cerr << "Couldn't allocate context\n";
        return -1;
    }

    InitByteContext();

    int err = av_open_input_file(&ic, filename, fmt, 0, &params);
    if (err < 0)
    {
        cerr << "avformat error: " << err << endl;
        exit(1);
    }

    int ret = av_find_stream_info(ic);
    if (ret < 0)
    {
        cerr << "could not find codec parameters: " << filename << endl;
        exit(1);
    }

    fmt->flags &= ~AVFMT_NOFILE;

    bitrate = 0;
    fps = 0;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                bitrate += enc->bit_rate;

                fps = (double)enc->frame_rate / enc->frame_rate_base;

                float aspect_ratio;
                if (enc->sample_aspect_ratio.num == 0)
                    aspect_ratio = 0;
                else
                    aspect_ratio = av_q2d(enc->sample_aspect_ratio) * 
                                   enc->width / enc->height;

                if (aspect_ratio <= 0.0)
                    aspect_ratio = (float)enc->width / (float)enc->height;

                current_width = enc->width;
                current_height = enc->height;
                current_aspect = aspect_ratio;

                m_parent->SetVideoParams(ALIGN(enc->width, 16), 
                                         ALIGN(enc->height, 16), fps, 
                                         keyframedist, aspect_ratio);
             
                enc->error_resilience = 2;
                enc->workaround_bugs = FF_BUG_AUTODETECT;
                enc->error_concealment = 3;
                enc->idct_algo = 0;
                enc->debug = 0;
                enc->rate_emu = 0;

                if (!novideo && (enc->codec_id == CODEC_ID_MPEG1VIDEO || 
                    enc->codec_id == CODEC_ID_MPEG2VIDEO))
                {
#ifdef USING_XVMC
                    if (gContext->GetNumSetting("UseXVMC", 1))
                        enc->codec_id = CODEC_ID_MPEG2VIDEO_XVMC;
#endif            
#ifdef USING_VIASLICE
                    if (gContext->GetNumSetting("UseViaSlice", 1))
                        enc->codec_id = CODEC_ID_MPEG2VIDEO_VIA;
#endif
                }

                AVCodec *codec = avcodec_find_decoder(enc->codec_id);

                if (codec && codec->id == CODEC_ID_MPEG2VIDEO_XVMC)
                {
                    enc->flags |= CODEC_FLAG_EMU_EDGE;
                    enc->get_buffer = get_avf_buffer_xvmc;
                    enc->release_buffer = release_avf_buffer_xvmc;
                    enc->draw_horiz_band = render_slice_xvmc;
                    enc->opaque = (void *)this;
                    directrendering = true;
                    enc->slice_flags = SLICE_FLAG_CODED_ORDER |
                                       SLICE_FLAG_ALLOW_FIELD;
                    setLowBuffers();
                    m_parent->ForceVideoOutputType(kVideoOutput_XvMC);
                }
                else if (codec && codec->id == CODEC_ID_MPEG2VIDEO_VIA)
                {
                    enc->flags |= CODEC_FLAG_EMU_EDGE;
                    enc->get_buffer = get_avf_buffer_via;
                    enc->release_buffer = release_avf_buffer_via;
                    enc->draw_horiz_band = render_slice_via;
                    enc->opaque = (void *)this;
                    directrendering = true;
                    enc->slice_flags = SLICE_FLAG_CODED_ORDER |
                                       SLICE_FLAG_ALLOW_FIELD;
                    setLowBuffers();
                    m_parent->ForceVideoOutputType(kVideoOutput_VIA);
                }
                else if (codec && codec->capabilities & CODEC_CAP_DR1 && 
                    !(enc->width % 16))
                {
                    enc->flags |= CODEC_FLAG_EMU_EDGE;
                    enc->draw_horiz_band = NULL;
                    enc->get_buffer = get_avf_buffer;
                    enc->release_buffer = release_avf_buffer;
                    enc->opaque = (void *)this;
                    directrendering = true;
                    hasbframes = enc->has_b_frames;
                }
                break;
            }
            case CODEC_TYPE_AUDIO:
            {
                bitrate += enc->bit_rate;
                m_parent->SetEffDsp(enc->sample_rate * 100);
                if (do_ac3_passthru && enc->codec_id == CODEC_ID_AC3) 
                {
                    // An AC3 stream looks like a 48KHz 2ch audio stream to 
                    // the sound card 
                    audio_sample_size = 4;
                    audio_sampling_rate = 48000;
                    audio_channels = 2;
                }
                else
                {
                    audio_sample_size = enc->channels * 2;
                    audio_sampling_rate = enc->sample_rate;
                    audio_channels = enc->channels;
                }
                m_parent->SetAudioParams(16, audio_channels, 
                                         audio_sampling_rate);
                break;
            }
            case CODEC_TYPE_DATA:
            {
                bitrate += enc->bit_rate;
                // otherwise, ignore it
                continue;
                break;
            }
            default:
            {
                cerr << "Unknown codec type: " << enc->codec_type << endl;
                break;
            }
        }

        AVCodec *codec;
        codec = avcodec_find_decoder(enc->codec_id);
        if (!codec)
        {
            cerr << "Unknown codec: " << enc->codec_id << endl;
            exit(1);
        }

        if (avcodec_open(enc, codec) < 0)
        {
            cerr << "Couldn't find lavc codec\n";
            exit(1);
        }
    }

    ringBuffer->CalcReadAheadThresh(bitrate);

    if ((m_playbackinfo && m_db) || livetv || watchingrecording)
    {
        recordingHasPositionMap = SyncPositionMap();
        if (recordingHasPositionMap && !livetv && !watchingrecording) 
        {
            hasFullPositionMap = true;
            gopset = true;
        }
    }

    if (!recordingHasPositionMap)
    {
        // the pvr-250 seems to overreport the bitrate by * 2
        float bytespersec = (float)bitrate / 8 / 2;
        float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
        m_parent->SetFileLength((int)(secs), (int)(secs * fps));

        // we will not see a position map from db or remote encoder,
        // so might as well use index by frames
        keyframedist = 1;
    }

    if (livetv || watchingrecording)
        ic->build_index = 0;
    
    dump_format(ic, 0, filename, 0);
    if (hasFullPositionMap)
    {
        VERBOSE(VB_PLAYBACK, "Position map found");
    }
    else if (recordingHasPositionMap)
    {
        VERBOSE(VB_PLAYBACK, "Partial position map found");
    }

    return recordingHasPositionMap;
}

bool AvFormatDecoder::CheckVideoParams(int width, int height)
{
    if (width == current_width && height == current_height)
        return false;

    cerr << "Video has changed: " << width << " " << height << " from: "
         << current_width << " " << current_height << endl;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                AVCodec *codec = enc->codec;
                avcodec_close(enc);
                avcodec_open(enc, codec);
                break;
            }
            default:
                break;
        }
    }

    return true;
}

bool AvFormatDecoder::CheckAudioParams(int freq, int channels)
{
    if (audio_check_1st == 2)
    {
        if (freq == audio_sampling_rate && channels == audio_channels)
            return false;

        audio_check_1st = 1;
        audio_sampling_rate_2nd = freq;
        audio_channels_2nd = channels;
        return false;
    }
    else
    {
        if (freq != audio_sampling_rate_2nd || channels != audio_channels_2nd ||
            (freq == audio_sampling_rate && channels == audio_channels))
        {
            audio_sampling_rate_2nd = -1;
            audio_channels_2nd = -1;
            audio_check_1st = 2;
            return false;
        }

        if (audio_check_1st == 1)
        {
            audio_check_1st = 0;
            return false;
        }

        audio_check_1st = 2;
    }

    QString chan = "stereo";
    if (channels == 1)
        chan = "mono";

    cerr << "Audio has changed: " << freq << "hz " << chan << endl;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_AUDIO:
            {
                AVCodec *codec = enc->codec;
                avcodec_close(enc);
                avcodec_open(enc, codec);
                break;
            }
            default:
                break;
        }
    }

    return true;
}

// returns true iff found exactly
// searches position if search_pos, index otherwise
bool AvFormatDecoder::FindPosition(long long desired_value, bool search_pos,
                                   int &lower_bound, int &upper_bound)
{
    // Binary search
    int upper = m_positionMap.size(), lower = -1;
    if (!search_pos && keyframedist > 0)
        desired_value /= keyframedist;

    while (upper - 1 > lower) 
    {
        int i = (upper + lower) / 2;
        long long value;
        if (search_pos)
            value = m_positionMap[i].pos;
        else
            value = m_positionMap[i].index;
        if (value == desired_value) 
        {
            // found it
            upper_bound = i;
            lower_bound = i;
            return true;
        }
        else if (value > desired_value)
            upper = i;
        else
            lower = i;
    }
    // Did not find it exactly -- return bounds

    if (search_pos) 
    {
        while (lower >= 0 && m_positionMap[lower].pos > desired_value)
            lower--;
        while (upper < (int)m_positionMap.size() &&
               m_positionMap[upper].pos > desired_value)
            upper++;
    }
    else
    {
        while (lower >= 0 &&m_positionMap[lower].index > desired_value)
            lower--;
        while (upper < (int)m_positionMap.size() && 
               m_positionMap[upper].index < desired_value)
            upper++;
    }
    // keep in bounds
    if (lower < 0)
        lower = 0;
    if (upper >= (int)m_positionMap.size())
        upper = (int)m_positionMap.size() - 1;

    upper_bound = upper;
    lower_bound = lower;
    return false;
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    VideoFrame *frame = nd->m_parent->GetNextVideoFrame();

    int width = frame->width;
    int height = frame->height;

    pic->data[0] = frame->buf;
    pic->data[1] = pic->data[0] + width * height;
    pic->data[2] = pic->data[1] + width * height / 4;

    pic->linesize[0] = width;
    pic->linesize[1] = width / 2;
    pic->linesize[2] = width / 2;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    nd->inUseBuffers.append(frame);

    return 1;
}

void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;

    if (pic->type == FF_BUFFER_TYPE_INTERNAL)
    {
        avcodec_default_release_buffer(c, pic);
        return;
    }

    assert(pic->type == FF_BUFFER_TYPE_USER);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->m_parent->GetNextVideoFrame();

    pic->data[0] = frame->priv[0];
    pic->data[1] = frame->priv[1];
    pic->data[2] = frame->buf;

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    nd->inUseBuffers.append(frame);

#ifdef USING_XVMC
    xvmc_render_state_t *render = (xvmc_render_state_t *)frame->buf;

    render->state = MP_XVMC_STATE_PREDICTION;
    render->picture_structure = 0;
    render->flags = 0;
    render->start_mv_blocks_num = 0;
    render->filled_mv_blocks_num = 0;
    render->next_free_data_block_num = 0;
#endif

    return 1;
}

void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;

    assert(pic->type == FF_BUFFER_TYPE_USER);

#ifdef USING_XVMC
    xvmc_render_state_t *render = (xvmc_render_state_t *)pic->data[2];
    render->state &= ~MP_XVMC_STATE_PREDICTION;
#endif

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;

}

void render_slice_xvmc(struct AVCodecContext *s, const AVFrame *src, 
                       int offset[4], int y, int type, int height)
{
    (void)offset;
    (void)type;

    AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);

    int width = s->width;

    VideoFrame *frame = (VideoFrame *)src->opaque;
    nd->m_parent->DrawSlice(frame, 0, y, width, height);
}

int get_avf_buffer_via(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    VideoFrame *frame = nd->m_parent->GetNextVideoFrame();

    pic->data[0] = frame->buf;
    pic->data[1] = NULL;
    pic->data[2] = NULL;

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    nd->inUseBuffers.append(frame);

    return 1;
}

void release_avf_buffer_via(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;

    assert(pic->type == FF_BUFFER_TYPE_USER);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

void render_slice_via(struct AVCodecContext *s, const AVFrame *src, 
                      int offset[4], int y, int type, int height)
{
    (void)offset;
    (void)type;

    if (!s || !src)
        return;

    AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);

    int width = s->width;

    VideoFrame *frame = (VideoFrame *)src->opaque;
    nd->m_parent->DrawSlice(frame, 0, y, width, height);
}

void AvFormatDecoder::HandleGopStart(AVPacket *pkt)
{
    if (prevgoppos != 0 && keyframedist != 1)
    {
        int tempKeyFrameDist = framesRead - 1 - prevgoppos;

        if (!gopset) // gopset: we've seen 2 keyframes
        {
            //cerr << "!gopset, syncing posmap" << endl;
            SyncPositionMap();
            if (tempKeyFrameDist > 0)
            {
                gopset = true;
                keyframedist = tempKeyFrameDist;
                //cerr << "Stream initial keyframedist: " << keyframedist << endl;
                m_parent->SetVideoParams(-1, -1, -1, keyframedist, 
                                         current_aspect);
            }
        }
        else
        {
            if (keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
            {
                //cerr << "KeyFrameDistance has changed to "
                //     << tempKeyFrameDist << " from " << keyframedist << endl;
                keyframedist = tempKeyFrameDist;
                m_parent->SetVideoParams(-1, -1, -1, keyframedist, 
                                         current_aspect);
                // also reset length
                long long index = m_positionMap[m_positionMap.size() - 1].index;
                long long totframes = index * keyframedist;
                int length = (int)((totframes * 1.0) / fps);
                cerr << "Setting(2) length to " << length
                     << " " << totframes << endl;
                m_parent->SetFileLength(length, totframes);
            }
        }
    }

    lastKey = prevgoppos = framesRead - 1;

    if (!hasFullPositionMap)
    {
        long long last_frame = 0;
        if (!m_positionMap.empty())
            last_frame = m_positionMap[m_positionMap.size() - 1].index;
        if (keyframedist > 1)
            last_frame *= keyframedist;
        // if we don't have an entry, fill it in with what we've just parsed
        if (framesRead > last_frame && keyframedist > 0)
        {
            long long startpos = pkt->startpos;

            // cerr << "positionMap[" << prevgoppos / keyframedist <<
            //         "] = " << startpos << "." << endl;
            // Grow positionMap vector several entries at a time
            if (m_positionMap.capacity() == m_positionMap.size())
                m_positionMap.reserve(m_positionMap.size() + 60);
            PosMapEntry entry = {prevgoppos / keyframedist, startpos};
            m_positionMap.push_back(entry);
        }

        // If we are > 150 frames in and saw no positionmap at all, reset 
        // length based on the actual bitrate seen so far
        if (framesRead > 150 && !recordingHasPositionMap && !livetv)
        {
            bitrate = (int)((pkt->startpos * 8 * fps) / (framesRead - 1));
            float bytespersec = (float)bitrate / 8;
            float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
            m_parent->SetFileLength((int)(secs), (int)(secs * fps));
        }
    }
}

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void AvFormatDecoder::MpegPreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = &(stream->codec);
    unsigned char *bufptr = pkt->data;
    unsigned int state = 0xFFFFFFFF, v = 0;
    
    int prvcount = -1;

    while (bufptr < pkt->data + pkt->size)
    {
        if (++prvcount < 3)
            v = prvpkt[prvcount];
        else
            v = *bufptr++;

        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                continue;

            switch (state)
            {
                case SEQ_START:
                {
                    unsigned char *test = bufptr;
                    int width = (test[0] << 4) | (test[1] >> 4);
                    int height = ((test[1] & 0xff) << 8) | test[2];

                    int aspectratioinfo = (test[3] >> 4);

                    float aspect = GetMpegAspect(context, aspectratioinfo,
                                                 width, height);

                    if (CheckVideoParams(width, height) ||
                        aspect != current_aspect)
                    {
                        m_parent->SetVideoParams(ALIGN(width,16),
                                                 ALIGN(height,16), fps,
                                                 keyframedist, aspect);
                        m_parent->ReinitVideo();
                        current_width = width;
                        current_height = height;
                        current_aspect = aspect;

                        gopset = false;
                        prevgoppos = 0;
                        video_last_P_pts = lastapts = lastvpts = 0;
                    }

                    seq_count++;
 
                    if (!seen_gop && seq_count > 1)
                        HandleGopStart(pkt);
                }
                break;

                case GOP_START:
                {
                    HandleGopStart(pkt);
                    seen_gop = true;
                }
                break;
            }
            continue;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }
    
    memcpy(prvpkt, pkt->data + pkt->size - 3, 3);
}

static const float avfmpeg1_aspect[16]={
    0.0000,    1.0000,    0.6735,    0.7031,
    0.7615,    0.8055,    0.8437,    0.8935,
    0.9157,    0.9815,    1.0255,    1.0695,
    1.0950,    1.1575,    1.2015,
};

static const float avfmpeg2_aspect[16]={
    0,    1.0,    -3.0/4.0,    -9.0/16.0,    -1.0/2.21,
};

float AvFormatDecoder::GetMpegAspect(AVCodecContext *context,
                                     int aspect_ratio_info,
                                     int width, int height)
{
    float retval = 0;

    if (aspect_ratio_info > 15)
        aspect_ratio_info = 15;
    if (aspect_ratio_info < 0)
        aspect_ratio_info = 0;

    if (context->sub_id == 1) // mpeg1
    {
        float aspect = avfmpeg1_aspect[aspect_ratio_info];
        if (aspect != 0.0)
            retval = width / (aspect * height);
    }
    else
    {
        float aspect = avfmpeg2_aspect[aspect_ratio_info];
        if (aspect > 0)
            retval = width / (aspect * height);
        else if (aspect < 0)
            retval = -1.0 / aspect;
    }

    if (retval <= 0)
        retval = width * 1.0 / height;

    return retval;
}

void AvFormatDecoder::GetFrame(int onlyvideo)
{
    AVPacket *pkt = NULL;
    int len, ret = 0;
    unsigned char *ptr;
    short samples[AVCODEC_MAX_AUDIO_FRAME_SIZE / 2];
    int data_size = 0;
    long long pts;
    bool firstloop = false;

    gotvideo = false;

    frame_decoded = 0;

    bool allowedquit = false;
    bool storevideoframes = false;

    while (!allowedquit)
    {
        if (gotvideo)
        {
            if (lowbuffers && onlyvideo == 0 && lastapts < lastvpts + 100 &&
                lastapts > lastvpts - 10000)
            {
                //cout << "behind: " << lastapts << " " << lastvpts << endl;
                storevideoframes = true;
            }
            else
            {
                allowedquit = true;
                continue;
            }
        }

        if (!storevideoframes && storedPackets.count() > 0)
        {
            if (pkt)
            {
                av_free_packet(pkt);
                delete pkt;
            }
            pkt = storedPackets.first();
            storedPackets.removeFirst();
        }
        else
        {
            if (!pkt)
                pkt = new AVPacket;

            if (av_read_frame(ic, pkt) < 0)
            {
                ateof = true;
                m_parent->SetEof();
                return;
            }
        }

        if (pkt->stream_index > ic->nb_streams)
        {
            cerr << "bad stream\n";
            av_free_packet(pkt);
            continue;
        }

        len = pkt->size;
        ptr = pkt->data;
        pts = 0;
        if (pkt->pts != (int64_t)AV_NOPTS_VALUE)
            pts = pkt->pts / (AV_TIME_BASE / 1000);

        AVStream *curstream = ic->streams[pkt->stream_index];

        if (storevideoframes && 
            curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            av_dup_packet(pkt);
            storedPackets.append(pkt);
            pkt = NULL;
            continue;
        }

        if (len > 0 && curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            framesRead++;
            if (exitafterdecoded)
                gotvideo = 1;

            AVCodecContext *context = &(curstream->codec);

            if (context->codec_id == CODEC_ID_MPEG1VIDEO ||
                context->codec_id == CODEC_ID_MPEG2VIDEO ||
                context->codec_id == CODEC_ID_MPEG2VIDEO_XVMC ||
                context->codec_id == CODEC_ID_MPEG2VIDEO_VIA)
            {
                MpegPreProcessPkt(curstream, pkt);
            }
        }

        if (!curstream->codec.codec)
        {
            cerr << "no codec for stream index " << pkt->stream_index << endl;
            av_free_packet(pkt);
            continue;
        }
 
        firstloop = true;

        while (len > 0)
        {
            switch (curstream->codec.codec_type)
            {
                case CODEC_TYPE_AUDIO:
                {
                    if (firstloop && pkt->pts != (int64_t)AV_NOPTS_VALUE)
                        lastapts = pkt->pts / (AV_TIME_BASE / 1000);

                    if (onlyvideo != 0)
                    {
                        ptr += pkt->size;
                        len -= pkt->size;
                        continue;
                    }

                    if (do_ac3_passthru)
                    {
                        data_size = pkt->size;
                        ret = EncodeAC3Frame(ptr, len, samples, data_size);
                    }
                    else
                    {
                        ret = avcodec_decode_audio(&curstream->codec, samples,
                                                   &data_size, ptr, len);
                    }

                    ptr += ret;
                    len -= ret;

                    if (data_size <= 0)
                        continue;

                    if (!do_ac3_passthru)
                    {
                        if (CheckAudioParams(curstream->codec.sample_rate,
                                             curstream->codec.channels))
                        {
                            audio_sampling_rate = curstream->codec.sample_rate;
                            audio_channels = curstream->codec.channels;
                            audio_sample_size = audio_channels * 2;

                            m_parent->SetEffDsp(audio_sampling_rate * 100);
                            m_parent->SetAudioParams(16, audio_channels,
                                                     audio_sampling_rate);
                            m_parent->ReinitAudio();
                        }
                    }

                    long long temppts = lastapts;
                    // calc for next frame
                    lastapts += (long long)((double)(data_size * 1000) /
                                audio_sample_size / audio_sampling_rate);

                    m_parent->AddAudioData((char *)samples, data_size, 
                                           temppts);
                    break;
                }
                case CODEC_TYPE_VIDEO:
                {
                    if (onlyvideo < 0)
                    {
                        ptr += pkt->size;
                        len -= pkt->size;
                        continue;
                    }

                    AVCodecContext *context = &(curstream->codec);
                    AVFrame mpa_pic;

                    int gotpicture = 0;

                    pthread_mutex_lock(&avcodeclock);
                    ret = avcodec_decode_video(context, &mpa_pic,
                                               &gotpicture, ptr, len);
                    pthread_mutex_unlock(&avcodeclock);

                    if (ret < 0)
                    {
                        cerr << "decoding error\n";
                        exit(1);
                    }

                    if (!gotpicture)
                    {
                        ptr += ret;
                        len -= ret;
                        continue;
                    }

                    VideoFrame *picframe = (VideoFrame *)(mpa_pic.opaque);

                    if (!directrendering)
                    {
                        AVPicture tmppicture;
                        AVPicture mpa_pic_p;

                        for (int i = 0; i < 4; i++)
                        {
                            mpa_pic_p.data[i] = mpa_pic.data[i];
                            mpa_pic_p.linesize[i] = mpa_pic.linesize[i];
                        }

                        picframe = m_parent->GetNextVideoFrame();
                        avpicture_fill(&tmppicture, picframe->buf, 
                                       PIX_FMT_YUV420P,
                                       context->width,
                                       context->height);
                        img_convert(&tmppicture, PIX_FMT_YUV420P, &mpa_pic_p,
                                    context->pix_fmt,
                                    context->width,
                                    context->height);
                    }

                    long long temppts = pts;

                    if (context->has_b_frames && mpa_pic.pict_type != FF_B_TYPE)
                    {
                        temppts = video_last_P_pts;
                        video_last_P_pts = pts;
                    }

                    if (temppts != 0)
                        lastvpts = temppts;
                    else
                        temppts = lastvpts;

                    // update video clock for next frame
                    int frame_delay = (int)(1000.0 / fps);

                    if (mpa_pic.repeat_pict)
                    {
                        //cerr << "repeat, unhandled?\n";
                        frame_delay += (int)(mpa_pic.repeat_pict * 
                                             frame_delay * 0.5);
                    }

                    lastvpts += frame_delay;

/* XXX: Broken.
                    if (mpa_pic.qscale_table != NULL && mpa_pic.qstride > 0 &&
                        context->height == picframe->height)
                    {
                        int tblsize = mpa_pic.qstride * 
                                      ((picframe->height + 15) / 16);

                        if (picframe->qstride != mpa_pic.qstride ||
                            picframe->qscale_table == NULL)
                        {
                            picframe->qstride = mpa_pic.qstride;
                            if (picframe->qscale_table)
                                delete [] picframe->qscale_table;
                            picframe->qscale_table = new unsigned char[tblsize];
                        }

                        memcpy(picframe->qscale_table, mpa_pic.qscale_table,
                               tblsize);
                    }
*/

                    m_parent->ReleaseNextVideoFrame(picframe, temppts);
                    if (directrendering)
                        inUseBuffers.removeRef(picframe);
                    gotvideo = 1;
                    framesPlayed++;

                    break;
                }
                default:
                    cerr << "error decoding - " << curstream->codec.codec_type 
                         << endl;
                    exit(1);
            }

            ptr += ret;
            len -= ret;
            frame_decoded = 1;
            firstloop = false;
        }
       
        av_free_packet(pkt);
    }                    

    if (pkt)
        delete pkt;

    m_parent->SetFramesPlayed(framesPlayed);
}

bool AvFormatDecoder::DoRewind(long long desiredFrame)
{
    if (m_positionMap.empty())
        return false;
    // Find keyframe <= desiredFrame, store in lastKey (frames)
    int pos_idx1, pos_idx2;

    FindPosition(desiredFrame, false, pos_idx1, pos_idx2);

    int pos_idx = pos_idx1 < pos_idx2 ? pos_idx1 : pos_idx2;

    lastKey = m_positionMap[pos_idx].index * keyframedist;

    long long keyPos = m_positionMap[pos_idx].pos;
    long long curPosition = ringBuffer->GetTotalReadPosition();
    int normalframes = desiredFrame - lastKey;
    long long diff = keyPos - curPosition;

    // Don't rewind further than we have space to store video
    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        pos_idx++;
        if (pos_idx >= (int)m_positionMap.size())
        {
            diff = 0;
            normalframes = 0;
            return false;
        }
        lastKey = m_positionMap[pos_idx].index * keyframedist;
        keyPos = m_positionMap[pos_idx].pos;

        diff = keyPos - curPosition;
        normalframes = 0;
    }

    ringBuffer->Seek(diff, SEEK_CUR);
    
    framesPlayed = lastKey;
    framesRead = lastKey;

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    SeekReset();

    while (normalframes > 0)
    {
        GetFrame(0);
        normalframes--;
    }

    m_parent->SetFramesPlayed(framesPlayed);
    return true;
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame)
{
    //cerr << "FF to " << desiredFrame << endl;
    // if the positionmap does not cover the desired frame, try to
    // refill.
    long long last_frame = 0;
    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

    if (desiredFrame > last_frame) 
    {
        //cerr << "FF syncing position map" << endl;
        SyncPositionMap();
    }

    //cerr << "Last frame: " << last_frame << endl;
    bool needflush = false;
    // if we still don't cover, parse forward in the stream one frame
    // at a time until we do

    if (!m_positionMap.empty())
        last_frame = m_positionMap[m_positionMap.size()-1].index * keyframedist;

    //if (desiredFrame > last_frame) 
    //cerr << "Map still doesn't cover" << endl;

    while (desiredFrame > last_frame)
    {
        needflush = true;
        
        exitafterdecoded = true;
        GetFrame(-1);
        exitafterdecoded = false;

        if (!m_positionMap.empty())
            last_frame = m_positionMap[m_positionMap.size()-1].index * 
                                                                  keyframedist; 
        //cerr << "FF seeking by 1 frame, now have " << last_frame << endl;
        
        if (ateof)
            return false;
    }

    if (m_positionMap.empty())
        return false;

    // Find keyframe >= desiredFrame, store in lastKey
    // if exactseeks, use keyframe <= desiredFrame
    int pos_idx1, pos_idx2;
    FindPosition(desiredFrame, false, pos_idx1, pos_idx2);
    if (pos_idx1 > pos_idx2) 
    {
        int tmp = pos_idx1;
        pos_idx1 = pos_idx2;
        pos_idx2 = tmp;
    }

    int pos_idx = exactseeks ? pos_idx1 : pos_idx2;
    lastKey = m_positionMap[pos_idx].index * keyframedist;
    long long keyPos = m_positionMap[pos_idx].pos;
    
    long long number = desiredFrame - framesPlayed;
    long long desiredKey = lastKey;

    int normalframes = desiredFrame - desiredKey;

    if (desiredKey == lastKey)
        normalframes = number;

    if (framesPlayed < lastKey)
    {
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        needflush = true;
    
        framesPlayed = lastKey;
        framesRead = lastKey;
    }
    else
    {
        //cerr << "Already at proper keyframe, framesplayed: " << framesPlayed
        //     << " pos1: " << pos_idx1 << " pos2: " << pos_idx2 << endl;
    }

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    if (needflush)
        SeekReset();

    while (normalframes > 0)
    {
        GetFrame(0);
        if (ateof)
            break;
        normalframes--;
    }

    m_parent->SetFramesPlayed(framesPlayed);
    return true;
}

void AvFormatDecoder::SetPositionMap(void)
{
    QMap<long long, long long> posMap;
    for (unsigned int i=0; i < m_positionMap.size(); i++) 
        posMap[m_positionMap[i].index] = m_positionMap[i].pos;
    
    if (m_playbackinfo && m_db) 
    {
        if (positionMapByFrame)
            m_playbackinfo->SetPositionMap(posMap, MARK_GOP_BYFRAME, m_db);
        else
            m_playbackinfo->SetPositionMap(posMap, MARK_GOP_START, m_db);
    }
}

bool AvFormatDecoder::PosMapFromDb()
{
    // Overwrites current positionmap with entire contents of database
    QMap<long long, long long> posMap;
    m_playbackinfo->GetPositionMap(posMap, MARK_GOP_BYFRAME, m_db);
    if (!posMap.empty())
    {
        positionMapByFrame = true;
        if (keyframedist == -1) 
        {
            //cerr << "keyframedist to 1 due to BYFRAME" << endl;
            keyframedist = 1;
        }
    }
    else
    {
        // Fall back to map by keyframe number, which assumes an
        // even cadence of keyframes
        m_playbackinfo->GetPositionMap(posMap, MARK_GOP_START, m_db);
    }

    if (posMap.empty())
        return false; // no position map in recording

    if (keyframedist == -1)
    {
        //cerr << "keyframedist to 15/12 due to GOP_START" << endl;
        keyframedist = 15;
        if (fps < 26 && fps > 24)
            keyframedist = 12;
    }

    m_positionMap.clear();
    m_positionMap.reserve(posMap.size());

    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.data()};
        m_positionMap.push_back(e);
    }

    return true;
}

bool AvFormatDecoder::PosMapFromEnc()
{
    // Reads only new positionmap entries from encoder
    if (!(livetv || (nvr_enc && nvr_enc->IsValidRecorder())))
        return false;

    QMap<long long, long long> posMap;
    
    int start = 0;
    unsigned int size = m_positionMap.size();
    if (size > 0)
        start = m_positionMap[size-1].index + 1;

    int end = nvr_enc->GetFramesWritten();
    if (size > 0 && keyframedist > 0) 
        end /= keyframedist;

    nvr_enc->FillPositionMap(start, end, posMap);
    if (keyframedist == -1 && posMap.size() > 1)
    {
        // If the indices are sequential, index is by keyframe num
        // else it is by frame num
        QMap<long long,long long>::const_iterator i1 = posMap.begin();
        QMap<long long,long long>::const_iterator i2 = i1;
        i2++;
        if (i1.key() + 1 == i2.key()) 
        {
            //cerr << "keyframedist to 15/12 due to encoder" << endl;
            keyframedist = 15;
            if (fps < 26 && fps > 24)
                keyframedist = 12;
        }
        else
        {
            //cerr << "keyframedist to 1 due to encoder" << endl;
            keyframedist = 1;
        }
    }

    // append this new position map to class's
    m_positionMap.reserve(m_positionMap.size() + posMap.size());
    for (QMap<long long,long long>::const_iterator it = posMap.begin();
         it != posMap.end(); it++) 
    {
        PosMapEntry e = {it.key(), it.data()};
        m_positionMap.push_back(e);
    }
    return true;
}

bool AvFormatDecoder::SyncPositionMap()
{
    // positionmap sources:
    // live tv:
    // 1. remote encoder
    // 2. stream parsing (in MpegPreProcessPkt)
    // decide keyframedist based on samples from remote encoder
    //
    // watching recording:
    // 1. initial fill from db
    // 2. incremental from remote encoder, until it finishes recording
    // 3. then db again (which should be the final time)
    // 4. stream parsing (in MpegPreProcessPkt)
    // decide keyframedist based on which table in db
    //
    // watching prerecorded:
    // 1. initial fill from db is all that's needed

    //cerr << "Resyncing position map" << endl;
    unsigned int old_posmap_size = m_positionMap.size();
    
    if (livetv)
    {
        PosMapFromEnc();
        //cerr << "Live TV: from encoder: " << m_positionMap.size() 
        //<< " entries" << endl;
    }
    else if (watchingrecording)
    {
        //cerr << "Watching & recording..." << endl;
        if (!posmapStarted) 
        {
            // starting up -- try first from database
            PosMapFromDb();
            //cerr << "from db: " << m_positionMap.size() << " entries" << endl;
        }
        // always try to get more from encoder
        if (!PosMapFromEnc()) 
        {
            //cerr << "...none from encoder" << endl;
            PosMapFromDb(); // try again from db
        }
        //cerr << "..." << m_positionMap.size() << " total" << endl;
    }
    else
    {
        // watching prerecorded ... just get from db
        if (!posmapStarted)
        {
            PosMapFromDb();
            //cerr << "Prerecorded... from db: " << m_positionMap.size() 
            //<< " (posmapStarted: " << posmapStarted << ")" << endl;
        }
    }

    bool ret_val = m_positionMap.size() > old_posmap_size;
    if (ret_val && keyframedist > 0)
    {
        long long totframes = 
            m_positionMap[m_positionMap.size()-1].index * keyframedist;
        int length = (int)((totframes * 1.0) / fps);
        m_parent->SetFileLength(length, totframes);
        m_parent->SetVideoParams(-1, -1, -1, keyframedist, current_aspect);
        posmapStarted = true;
    }
    return ret_val;
}

void AvFormatDecoder::setWatchingRecording(bool mode)
{
    // When we switch from WatchingRecording to WatchingPreRecorded,
    // re-get the positionmap
    posmapStarted = false;
    DecoderBase::setWatchingRecording(mode);
}

int AvFormatDecoder::EncodeAC3Frame(unsigned char *data, int len,
                                    short *samples, int &samples_size)
{
    int enc_len;
    int flags, sample_rate, bit_rate;
    unsigned char* ucsamples = (unsigned char*) samples;

    // we don't do any length/crc validation of the AC3 frame here; presumably
    // the receiver will have enough sense to do that.  if someone has a
    // receiver that doesn't, here would be a good place to put in a call
    // to a52_crc16_block(samples+2, data_size-2) - but what do we do if the
    // packet is bad?  we'd need to send something that the receiver would
    // ignore, and if so, may as well just assume that it will ignore
    // anything with a bad CRC...

    enc_len = a52_syncinfo(data, &flags, &sample_rate, &bit_rate);

    if (enc_len == 0 || enc_len > len)
    {
        samples_size = 0;
        return len;
    }

    if (enc_len > MAX_AC3_FRAME_SIZE - 8)
        enc_len = MAX_AC3_FRAME_SIZE - 8;

    swab(data, ucsamples + 8, enc_len);

    // the following values come from ao_hwac3.c in mplayer.
    // they form a valid IEC958 AC3 header.
    ucsamples[0] = 0x72;
    ucsamples[1] = 0xF8;
    ucsamples[2] = 0x1F;
    ucsamples[3] = 0x4E;
    ucsamples[4] = 0x01;
    ucsamples[5] = 0x00;
    ucsamples[6] = (enc_len << 3) & 0xFF;
    ucsamples[7] = (enc_len >> 5) & 0xFF;
    memset(ucsamples + 8 + enc_len, 0, MAX_AC3_FRAME_SIZE - 8 - enc_len);
    samples_size = MAX_AC3_FRAME_SIZE;

    return len;  // consume whole frame even if len > enc_len ?
}

