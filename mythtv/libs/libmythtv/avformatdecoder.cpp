#include <iostream>
using namespace std;

#include <assert.h>

#include "avformatdecoder.h"
#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "programinfo.h"

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

    audio_sample_size = 4;
    audio_sampling_rate = 48000;

    hasbframes = false;

    haspositionmap = false;

    keyframedist = 15;

    exitafterdecoded = false;
    ateof = false;
    gopset = 0;

    fps = 29.97;
    validvpts = false;
}

AvFormatDecoder::~AvFormatDecoder()
{
    if (ic)
    {
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

int AvFormatDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                              char testbuf[2048])
{
    (void)novideo;
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

                m_parent->SetVideoParams(enc->width, enc->height, fps, 
                                         keyframedist);
             
                enc->error_resilience = 2;
                enc->workaround_bugs = FF_BUG_AUTODETECT;
                enc->error_concealment = 3;
                enc->idct_algo = 0;
                enc->debug = 0;
                enc->rate_emu = 0;

                AVCodec *codec = avcodec_find_decoder(enc->codec_id);

                if (codec && codec->capabilities & CODEC_CAP_TRUNCATED)
                    enc->flags |= CODEC_FLAG_TRUNCATED;

                if (codec && codec->capabilities & CODEC_CAP_DR1 && 
                    enc->codec_id != CODEC_ID_SVQ3)
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

    if (m_playbackinfo && m_db)
    {
        m_playbackinfo->GetPositionMap(positionMap, MARK_GOP_START, m_db);
        if (positionMap.size() > 1)
        {
            haspositionmap = true;
            long long totframes = positionMap.size() * keyframedist;
            int length = (int)((totframes * 1.0) / fps);
            m_parent->SetFileLength(length, totframes);            
            gopset = true;
        }
    }

    if (!haspositionmap)
    {
        // the pvr-250 seems to overreport the bitrate by * 2
        float bytespersec = (float)bitrate / 8 / 2;
        float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;

        m_parent->SetFileLength((int)(secs), (int)(secs * fps));
    }

    dump_format(ic, 0, filename, 0);
    if (haspositionmap)
        cout << "Position map found\n";

    return haspositionmap;
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

#define GOP_START     0x000001B8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

bool AvFormatDecoder::PacketHasHeader(unsigned char *buf, int len,
                                      unsigned int startcode)
{
    unsigned char *bufptr;
    unsigned int state = 0xFFFFFFFF, v;
    
    bufptr = buf;

    while (bufptr < buf + len)
    {
        v = *bufptr++;
        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            if (state >= SLICE_MIN && state <= SLICE_MAX)
                return false;
            if (state == startcode)
                return true;
        }
        state = ((state << 8) | v) & 0xFFFFFF;
    }

    return false;
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
            ateof = true;
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

        if (len > 0 && curstream->codec.codec_type == CODEC_TYPE_VIDEO)
        {
            AVCodecContext *context = &(curstream->codec);
            if (context->codec_id == CODEC_ID_MPEG1VIDEO)
            {
                if (PacketHasHeader(pkt.data, pkt.size, PICTURE_START))
                    framesRead++;

                if (PacketHasHeader(pkt.data, pkt.size, GOP_START))
                {
                    int frameNum = framesRead - 1;
 
                    if (!gopset && frameNum > 0)
                    {
                        keyframedist = frameNum;
                        gopset = true;
                        m_parent->SetVideoParams(-1, -1, -1, keyframedist);
                    }

                    lastKey = frameNum;
                    if (!haspositionmap)
                        positionMap[lastKey / keyframedist] = pkt.startpos;
                }
            }
        }
 
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

                    if (lastapts != pkt.pts && pkt.pts > 0 && validvpts)
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

                    if (ret == 0)
                    {
                        if (exitafterdecoded)
                            gotvideo = 1;
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
                        long long newvpts = 0;
                        if (pkt.pts > 0)
                        {
                            validvpts = true;
                            newvpts = (long long int)(pkt.pts * 1.0 * 
                                       ic->pts_num / (ic->pts_den / 1000));
                        }
                        else
                        {
                            // guess, based off of the audio timestamp and 
                            // the prebuffer size
                            newvpts = lastapts / 90 + (int)(1000.0 / fps) * 3;
                        }

                        if (newvpts <= lastvpts)
                            lastvpts += (int)(1000.0 / fps);
                        else
                            lastvpts = newvpts;
                    }
                    else
                        lastvpts += (int)(1000.0 / fps);

                    m_parent->ReleaseNextVideoFrame(true, lastvpts);
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
        
        av_free_packet(&pkt);
    }                    

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

    if (keyPos == 0)
    {
        cout << "unknown position: " << lastKey << endl;
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

    if (desiredKey != lastKey)
    {
        int desiredIndex = desiredKey / keyframedist;

        if (positionMap.find(desiredIndex) != positionMap.end())
        {
            keyPos = positionMap[desiredIndex];
        }
        else if (livetv || (watchingrecording && nvr_enc &&
                            nvr_enc->IsValidRecorder()))
        {
            for (int i = lastKey / keyframedist; i <= desiredIndex; i++)
            {
                if (positionMap.find(i) == positionMap.end())
                    nvr_enc->FillPositionMap(i, desiredIndex, positionMap);
            }
            keyPos = positionMap[desiredIndex];
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
    else if (desiredKey != lastKey)
    {
        AVCodecContext *videoenc = NULL;

        for (int i = 0; i < ic->nb_streams; i++)
        {
            AVCodecContext *enc = &ic->streams[i]->codec;
            if (enc->codec_type == CODEC_TYPE_VIDEO)
            {
                videoenc = enc;
                break;
            }
        }            

        if (!videoenc)
        {
            cerr << "Couldn't find video decoder\n";
            return false;
        }
 
        videoenc->hurry_up = 5;

        while (framesRead < desiredKey + 1 || 
               !positionMap.contains(desiredKey / keyframedist))
        {
            needflush = true;

            exitafterdecoded = true;
            GetFrame(1);
            exitafterdecoded = false;

            if (ateof)
                return false;
        }

        videoenc->hurry_up = 0;

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

char *AvFormatDecoder::GetScreenGrab(int secondsin)
{
    return NULL;
}

void AvFormatDecoder::SetPositionMap(void)
{
    if (m_playbackinfo && m_db)
        m_playbackinfo->SetPositionMap(positionMap, MARK_GOP_START, m_db);
}

