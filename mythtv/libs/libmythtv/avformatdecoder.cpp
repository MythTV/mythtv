#include <iostream>
#include <assert.h>

using namespace std;

#include "avformatdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcontext.h"

extern "C" {
#ifdef USING_XVMC
#include "../libavcodec/xvmc_render.h"
#endif
}

extern pthread_mutex_t avcodeclock;

AvFormatDecoder::AvFormatDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                                 ProgramInfo *pginfo)
               : DecoderBase(parent)
{
    m_db = db;
    m_playbackinfo = pginfo;

    ic = NULL;
    directrendering = false;
    lastapts = lastvpts = 0;
    framesPlayed = 0;
    framesRead = 0;

    audio_check_1st = 2;
    audio_sample_size = 4;
    audio_sampling_rate = 48000;

    hasbframes = false;

    hasFullPositionMap = false;

    keyframedist = 15;

    exitafterdecoded = false;
    ateof = false;
    gopset = false;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;
    validvpts = false;

    lastKey = 0;

    memset(prvpkt, 0, 3);
}

AvFormatDecoder::~AvFormatDecoder()
{
    if (ic)
    {
        for (int i = 0; i < ic->nb_streams; i++) 
        {
            AVStream *st = ic->streams[i];
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

    AVPacketList *pktl = NULL;
    while ((pktl = ic->packet_buffer))
    {
        ic->packet_buffer = pktl->next;
        av_free(pktl);
    }

    ic->pb.pos = ringBuffer->GetTotalReadPosition();
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
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

    positionMap.clear();
    framesPlayed = 0;
    framesRead = 0;
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
        exit(0);
    }

    int ret = av_find_stream_info(ic);
    if (ret < 0)
    {
        cerr << "could not find codec parameters: " << filename << endl;
        exit(0);
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

                if (fps < 26 && fps > 24)
                    keyframedist = 12;

                float aspect_ratio = av_q2d(enc->sample_aspect_ratio) * 
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
                    enc->codec_id = CODEC_ID_MPEG2VIDEO_XVMC;
#endif            
#ifdef USING_VIASLICE
                    enc->codec_id = CODEC_ID_MPEG2VIDEO_VIA;
#endif
                }

                AVCodec *codec = avcodec_find_decoder(enc->codec_id);

                if (codec && codec->capabilities & CODEC_CAP_TRUNCATED)
                    enc->flags |= CODEC_FLAG_TRUNCATED;

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
                m_parent->SetAudioParams(16, enc->channels, enc->sample_rate);
                audio_sample_size = enc->channels * 2;
                audio_sampling_rate = enc->sample_rate;
                audio_channels = enc->channels;
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
            exit(0);
        }

        if (avcodec_open(enc, codec) < 0)
        {
            cerr << "Couldn't find lavc codec\n";
            exit(0);
        }
    }

    ptsmultiplier = ((double)ic->pts_num) / (ic->pts_den / 1000.0);

    ringBuffer->CalcReadAheadThresh(bitrate);

    if (m_playbackinfo && m_db)
    {
        m_playbackinfo->GetPositionMap(positionMap, MARK_GOP_START, m_db);
        if (positionMap.size() && !livetv && !watchingrecording)
        {
            long long totframes = positionMap.size() * keyframedist;
            int length = (int)((totframes * 1.0) / fps);
            m_parent->SetFileLength(length, totframes);            
            hasFullPositionMap = true;
            gopset = true;
        }
    }

    if (!hasFullPositionMap)
    {
        // the pvr-250 seems to overreport the bitrate by * 2
        float bytespersec = (float)bitrate / 8 / 2;
        float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;

        m_parent->SetFileLength((int)(secs), (int)(secs * fps));
    }

    dump_format(ic, 0, filename, 0);
    if (hasFullPositionMap)
        VERBOSE(VB_PLAYBACK, "Position map found");

    return hasFullPositionMap;
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

    AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);

    int width = s->width;

    VideoFrame *frame = (VideoFrame *)src->opaque;
    nd->m_parent->DrawSlice(frame, 0, y, width, height);
}

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void AvFormatDecoder::MpegPreProcessPkt(AVCodecContext *context, AVPacket *pkt)
{
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
                        lastapts = lastvpts = 0;
                    }
                }
                break;

                case GOP_START:
                {
                    int tempKeyFrameDist = framesRead - 1 - prevgoppos;

                    if (!gopset)
                    {
                        if (prevgoppos > 0 && tempKeyFrameDist > 0)
                        {
                            gopset = true;
                            keyframedist = tempKeyFrameDist;
                            m_parent->SetVideoParams(-1, -1, -1, keyframedist);
                        }
                    }
                    else
                    {
                        if (keyframedist != tempKeyFrameDist && 
                            tempKeyFrameDist > 0)
                        {
                            //cerr << "KeyFrameDistance has changed to "
                            //     << tempKeyFrameDist << "." << endl;
                            keyframedist = tempKeyFrameDist;
                            m_parent->SetVideoParams(-1, -1, -1, keyframedist);
                        }
                    }

                    lastKey = prevgoppos = framesRead - 1;

                    if (!hasFullPositionMap)
                    {
                        //cerr << "positionMap[" << prevgoppos / keyframedist <<
                        //        "] = " << pkt->startpos << "." << endl;
                        positionMap[prevgoppos / keyframedist] = pkt->startpos;
                    }
                }
                break;

                case PICTURE_START:
                {
                    framesRead++;
                    if (exitafterdecoded)
                        gotvideo = 1;
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
    long long temppts;

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
                delete pkt;
            pkt = storedPackets.first();
            storedPackets.removeFirst();
        }
        else
        {
            if (!pkt)
                pkt = new AVPacket;

            if (av_read_packet(ic, pkt) < 0)
            {
                ateof = true;
                m_parent->SetEof();
                return;
            }
        }

        len = pkt->size;
        ptr = pkt->data;

        if (pkt->stream_index > ic->nb_streams)
        {
            cerr << "bad stream\n";
            av_free_packet(pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt->stream_index];

        if (storevideoframes && 
            curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            storedPackets.append(pkt);
            pkt = NULL;
            continue;
        }

        if (len > 0 && curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            AVCodecContext *context = &(curstream->codec);

            if (context->codec_id == CODEC_ID_MPEG1VIDEO ||
                context->codec_id == CODEC_ID_MPEG2VIDEO ||
                context->codec_id == CODEC_ID_MPEG2VIDEO_XVMC ||
                context->codec_id == CODEC_ID_MPEG2VIDEO_VIA)
            {
                MpegPreProcessPkt(context, pkt);
            }
        }
 
        while (len > 0)
        {
            switch (curstream->codec.codec_type)
            {
                case CODEC_TYPE_AUDIO:
                {
                    if (onlyvideo != 0)
                    {
                        ptr += pkt->size;
                        len -= pkt->size;
                        continue;
                    }

                    ret = avcodec_decode_audio(&curstream->codec, samples,
                                               &data_size, ptr, len);
                    if (data_size <= 0)
                    {
                        ptr += ret;
                        len -= ret;
                        continue;
                    }

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

                    temppts = (long long)((double)pkt->pts * ptsmultiplier); 

                    if (lastapts != temppts && temppts > 0 && validvpts)
                    {
                        lastapts = temppts;
                    }
                    else
                    {
                        lastapts += (long long)((double)(data_size * 1000) / 
                                    audio_sample_size / audio_sampling_rate);
                    }

                    m_parent->AddAudioData((char *)samples, data_size, 
                                           lastapts);
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
                        exit(0);
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

                    if (mpa_pic.pict_type == FF_I_TYPE ||
                        mpa_pic.pict_type == FF_P_TYPE)
                    {
                        long long newvpts = 0;
                        if (pkt->pts > 0)
                        {
                            validvpts = true;
                            newvpts = (long long)((double)pkt->pts * 
                                                  ptsmultiplier); 
                        }

                        if (newvpts <= lastvpts)
                            lastvpts += (int)(1000.0 / fps);
                        else
                            lastvpts = newvpts;
                    }
                    else
                        lastvpts += (int)(1000.0 / fps);

                    m_parent->ReleaseNextVideoFrame(picframe, lastvpts);
                    if (directrendering)
                        inUseBuffers.removeRef(picframe);
                    gotvideo = 1;
                    framesPlayed++;

                    break;
                }
                default:
                    cerr << "error decoding - " << curstream->codec.codec_type 
                         << endl;
                    exit(0);
            }

            ptr += ret;
            len -= ret;
            frame_decoded = 1;
        }
        
        av_free_packet(pkt);
    }                    

    if (pkt)
        delete pkt;

    m_parent->SetFramesPlayed(framesPlayed);
}

bool AvFormatDecoder::DoRewind(long long desiredFrame)
{
    lastKey = (framesPlayed / keyframedist) * keyframedist;
    long long storelastKey = lastKey;
    while (lastKey > desiredFrame)
    {
        lastKey -= keyframedist;
    }

    if (lastKey < 0)
        lastKey = 0;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = positionMap[lastKey / keyframedist];
    long long curPosition = ringBuffer->GetTotalReadPosition();
    long long diff = keyPos - curPosition;

    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += keyframedist;
        keyPos = positionMap[lastKey / keyframedist];
        if (keyPos == 0)
            continue;
        diff = keyPos - curPosition;
        normalframes = 0;
        if (lastKey > storelastKey)
        {
            lastKey = storelastKey;
            diff = 0;
            normalframes = 0;
            return false;
        }
    }

    int iter = 0;

    while (keyPos == 0 && lastKey > 1 && iter < 4)
    {
        VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                                     .arg((int)lastKey));

        lastKey -= keyframedist;
        keyPos = positionMap[lastKey / keyframedist];
        diff = keyPos - curPosition;
        normalframes += keyframedist;

        if (keyPos != 0)
           VERBOSE(VB_PLAYBACK, QString("Using seek position: %1")
                                       .arg((int)lastKey));

        iter++;
    }

    if (keyPos == 0)
    {
        VERBOSE(VB_GENERAL, QString("Unknown seek position: %1")
                                    .arg((int)lastKey));

        lastKey = storelastKey;
        diff = 0;
        normalframes = 0;

        return false;
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
    lastKey = (framesPlayed / keyframedist) * keyframedist;
    long long number = desiredFrame - framesPlayed;
    long long desiredKey = lastKey;

    while (desiredKey <= desiredFrame)
    {
        desiredKey += keyframedist;
    }
    desiredKey -= keyframedist;
   
    int normalframes = desiredFrame - desiredKey;

    if (desiredKey == lastKey)
        normalframes = number;

    long long keyPos = -1;

    long long tmpKey = desiredKey;
    int tmpIndex = tmpKey / keyframedist;

    while (keyPos == -1 && tmpKey > lastKey)
    {
        if (positionMap.find(tmpIndex) != positionMap.end())
        {
            keyPos = positionMap[tmpIndex];
        }
        else if (livetv || (watchingrecording && nvr_enc &&
                            nvr_enc->IsValidRecorder()))
        {
            for (int i = lastKey / keyframedist; i <= tmpIndex; i++)
            {
                if (positionMap.find(i) == positionMap.end())
                    nvr_enc->FillPositionMap(i, tmpIndex, positionMap);
            }
            keyPos = positionMap[tmpIndex];
        }
        if (keyPos == -1)
        {
            VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                    .arg((int)tmpKey));

            tmpKey -= keyframedist;
            tmpIndex--;
        }
        else
        {
            desiredKey = tmpKey;
        }
    }

    bool needflush = false;

    if (keyPos != -1)
    {
        lastKey = desiredKey;
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        needflush = true;

        framesPlayed = lastKey;
        framesRead = lastKey;
    }
    else if (desiredKey != lastKey && !livetv && !watchingrecording)
    {
        VERBOSE(VB_IMPORTANT, "Did not find keyframe position.");

        VERBOSE(VB_PLAYBACK, QString("lastKey: %1 desiredKey: %2")
                .arg((int)lastKey).arg((int)desiredKey));

        while (framesRead < desiredKey + 1 || 
               !positionMap.contains(desiredKey / keyframedist))
        {
            needflush = true;

            exitafterdecoded = true;
            GetFrame(-1);
            exitafterdecoded = false;

            if (ateof)
                return false;
        }

        if (needflush)
        {
            lastKey = desiredKey;
            keyPos = positionMap[desiredKey / keyframedist];
            long long diff = keyPos - ringBuffer->GetTotalReadPosition();

            ringBuffer->Seek(diff, SEEK_CUR);
        }

        framesPlayed = lastKey;
        framesRead = lastKey;
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
    if (m_playbackinfo && m_db)
        m_playbackinfo->SetPositionMap(positionMap, MARK_GOP_START, m_db);
}

