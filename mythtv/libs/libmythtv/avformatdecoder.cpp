#include <iostream>
using namespace std;

#include "avformatdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"

extern pthread_mutex_t avcodeclock;

AvFormatDecoder::AvFormatDecoder(NuppelVideoPlayer *parent)
               : DecoderBase(parent)
{
    ic = NULL;
    directrendering = false;
    pkt = NULL;
    lastapts = lastvpts = 0;
    framesPlayed = 0;

    audio_sample_size = 4;
    audio_sampling_rate = 48000;

    hasbframes = false;
}

AvFormatDecoder::~AvFormatDecoder()
{
    if (ic)
    {
        av_close_input_file(ic);
        ic = NULL;
    }
}

void AvFormatDecoder::Reset(void)
{
    lastapts = 0;
    lastvpts = 0;

    AVPacketList *pktl = NULL;
    while ((pktl = ic->packet_buffer))
    {
        *pkt = pktl->pkt;
        ic->packet_buffer = pktl->next;
        av_free(pktl);
    }

    ic->pb.pos = 0;
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        avcodec_flush_buffers(enc);
    }
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

    int bitrate = 0;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                bitrate += enc->bit_rate;

                double fps = (double)enc->frame_rate / enc->frame_rate_base;
                m_parent->SetVideoParams(enc->width, enc->height, fps, 30);
              
                enc->error_resilience = 2;
                enc->workaround_bugs = FF_BUG_AUTODETECT;
                enc->error_concealment = 3;
                enc->idct_algo = 0;
                enc->debug = 0;
                enc->rate_emu = 0;

                if (enc->codec_id == CODEC_ID_MPEG1VIDEO || 
                    enc->codec_id == CODEC_ID_H264)
                {
                    enc->flags|= CODEC_FLAG_TRUNCATED;
                }

                AVCodec *codec = avcodec_find_decoder(enc->codec_id);
                if (codec && codec->capabilities & CODEC_CAP_DR1)
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

    ringBuffer->CalcReadAheadThresh(bitrate);

    dump_format(ic, 0, filename, 0);

    return 0;
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    int width = c->width;
    int height = c->height;

    if (pic->reference && nd->hasbframes)
    {
       return avcodec_default_get_buffer(c, pic);
    }

    pic->data[0] = nd->directbuf;
    pic->data[1] = pic->data[0] + width * height;
    pic->data[2] = pic->data[1] + width * height / 4;

    pic->linesize[0] = width;
    pic->linesize[1] = width / 2;
    pic->linesize[2] = width / 2;

    pic->opaque = nd;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    return 1;
}

void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;
    //assert(pic->type == FF_BUFFER_TYPE_USER);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

void AvFormatDecoder::GetFrame(int onlyvideo)
{
    AVPacket pkt;
    int len, ret = 0;
    unsigned char *ptr;
    short samples[AVCODEC_MAX_AUDIO_FRAME_SIZE / 2];
    int data_size = 0;

    bool gotvideo = false;

    frame_decoded = 0;

    while (!gotvideo)
    {
        if (av_read_packet(ic, &pkt) < 0)
        {
            m_parent->SetEof();
            return;
        }

        len = pkt.size;
        ptr = pkt.data;
        bool pts_set = false;

        if (pkt.stream_index > ic->nb_streams)
        {
            cout << "bad stream\n";
            av_free_packet(&pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt.stream_index];
 
        while (len > 0)
        {
            long long ipts = AV_NOPTS_VALUE;

            if (frame_decoded && pkt.pts != AV_NOPTS_VALUE && !pts_set)
            {
                ipts = pkt.pts;
                pts_set = 1;
                frame_decoded = 0;
            }

            switch (curstream->codec.codec_type)
            {
                case CODEC_TYPE_AUDIO:
                {
                    if (onlyvideo)
                    {
                        ptr += pkt.size;
                        len -= pkt.size;
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

                    if (lastapts != pkt.pts)
                    {
                        lastapts = pkt.pts;
                    }
                    else
                    {
                        lastapts += (long long)((double)((data_size * 90.0) / 
                                    audio_sample_size / audio_sampling_rate)
                                    * 1000);
                    }
                    m_parent->AddAudioData((char *)samples, data_size, 
                                           lastapts / 90);
                    break;
                }
                case CODEC_TYPE_VIDEO:
                {
                    AVCodecContext *context = &(curstream->codec);
                    AVFrame mpa_pic;

                    unsigned char *buf = m_parent->GetNextVideoFrame();
                    int gotpicture = 0;

                    pthread_mutex_lock(&avcodeclock);
                    directbuf = buf;
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
                        m_parent->ReleaseNextVideoFrame(false, 0);
                        ptr += ret;
                        len -= ret;
                        continue;
                    }

                    if (!directrendering || (mpa_pic.reference && hasbframes))
                    {
                        AVPicture tmppicture;
                        AVPicture mpa_pic_p;

                        for (int i = 0; i < 4; i++)
                        {
                            mpa_pic_p.data[i] = mpa_pic.data[i];
                            mpa_pic_p.linesize[i] = mpa_pic.linesize[i];
                        }

                        avpicture_fill(&tmppicture, buf, PIX_FMT_YUV420P,
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
                        lastvpts = (long long int)(pkt.pts * 1.0 * 
                                   ic->pts_num / (ic->pts_den / 1000));
                    }
                    else
                        lastvpts += 33;

                    m_parent->ReleaseNextVideoFrame(true, lastvpts);
                    gotvideo = 1;
                    framesPlayed++;

                    break;
                }
                default:
                    cerr << "error decoding\n";
                    exit(0);
            }

            ptr += ret;
            len -= ret;
            frame_decoded = 1;
        }
        
        av_free_packet(&pkt);
    }                    

    m_parent->SetFramesPlayed(framesPlayed);
}

bool AvFormatDecoder::DoRewind(long long desiredFrame)
{
    return false;
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame)
{
    return false;
}

char *AvFormatDecoder::GetScreenGrab(int secondsin)
{
    return NULL;
}

