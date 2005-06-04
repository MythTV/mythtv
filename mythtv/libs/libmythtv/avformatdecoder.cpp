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
#include "mythdbcon.h"

extern "C" {
#ifdef USING_XVMC
#include "libavcodec/xvmc_render.h"
#endif
#include "libavcodec/liba52/a52.h"
#include "../libmythmpeg2/mpeg2.h"
}

#define MAX_AC3_FRAME_SIZE 6144

extern pthread_mutex_t avcodeclock;

class AvFormatDecoderPrivate
{
  public:
    AvFormatDecoderPrivate(AvFormatDecoder *lparent);
   ~AvFormatDecoderPrivate();
    
    bool InitMPEG2();
    void DestroyMPEG2();
    void ResetMPEG2();
    int DecodeMPEG2Video(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr, uint8_t *buf, int buf_size);
    
    AvFormatDecoder *parent;
    mpeg2dec_t *mpeg2dec;
};

AvFormatDecoderPrivate::AvFormatDecoderPrivate(AvFormatDecoder *lparent)
{
    parent = lparent;
    mpeg2dec = NULL;
}

AvFormatDecoderPrivate::~AvFormatDecoderPrivate()
{
    DestroyMPEG2();
}

bool AvFormatDecoderPrivate::InitMPEG2()
{
    DestroyMPEG2();
    if (gContext->GetNumSetting("UseMPEG2Dec", 0))
    {
        mpeg2dec = mpeg2_init();
        if (mpeg2dec)
            VERBOSE(VB_PLAYBACK, "Using libmpeg2 for video decoding");
    }
    return (mpeg2dec != NULL);
}

void AvFormatDecoderPrivate::DestroyMPEG2()
{
    if (mpeg2dec)
        mpeg2_close(mpeg2dec);
    mpeg2dec = NULL;
}

void AvFormatDecoderPrivate::ResetMPEG2()
{
    if (mpeg2dec)
        mpeg2_reset(mpeg2dec, 0);
}

int AvFormatDecoderPrivate::DecodeMPEG2Video(AVCodecContext *avctx,
                                             AVFrame *picture,
                                             int *got_picture_ptr,
                                             uint8_t *buf, int buf_size)
{
    *got_picture_ptr = 0;
    const mpeg2_info_t *info = mpeg2_info(mpeg2dec);
    mpeg2_buffer(mpeg2dec, buf, buf + buf_size);
    while (1)
    {
        switch (mpeg2_parse(mpeg2dec))
        {
            case STATE_SEQUENCE:
                // libmpeg2 needs three buffers to do its work.
                // We set up two prediction buffers here, from
                // the set of available video frames.
                mpeg2_custom_fbuf(mpeg2dec, 1);
                for (int i = 0; i < 2; i++)
                {
                    avctx->get_buffer(avctx, picture);
                    mpeg2_set_buf(mpeg2dec, picture->data, picture->opaque);
                }
                break;
            case STATE_PICTURE:
                // This sets up the third buffer for libmpeg2.
                // We use up one of the three buffers for each
                // frame shown. The frames get released once
                // they are drawn (outside this routine).
                avctx->get_buffer(avctx, picture);
                mpeg2_set_buf(mpeg2dec, picture->data, picture->opaque);
                break;
            case STATE_BUFFER:
                // We're supposed to wait for STATE_SLICE, but
                // STATE_BUFFER is returned first. Since we handed
                // libmpeg2 a full frame, we know it should be
                // done once it's finished with the data.
                if (info->display_fbuf)
                {
                    picture->data[0] = info->display_fbuf->buf[0];
                    picture->data[1] = info->display_fbuf->buf[1];
                    picture->data[2] = info->display_fbuf->buf[2];
                    picture->opaque  = info->display_fbuf->id;
                    *got_picture_ptr = 1;
                    picture->top_field_first = !!(info->display_picture->flags & PIC_FLAG_TOP_FIELD_FIRST);
                    picture->interlaced_frame = !(info->display_picture->flags & PIC_FLAG_PROGRESSIVE_FRAME);
                }
                return buf_size;
            case STATE_INVALID:
                // This is the error state. The decoder must be
                // reset on an error.
                mpeg2_reset(mpeg2dec, 0);
                return -1;
            default:
                break;
        }
    }
}


AvFormatDecoder::AvFormatDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo)
               : DecoderBase(parent, pginfo)
{
    d = new AvFormatDecoderPrivate(this);
    
    ic = NULL;
    directrendering = false;
    lastapts = lastvpts = 0;
    framesPlayed = 0;
    framesRead = 0;

    wantedAudioStream = -1;

    audio_check_1st = 2;
    audio_channels = -1;
    audio_sample_size = -1;
    audio_sampling_rate = -1;

    exitafterdecoded = false;
    ateof = false;
    gopset = false;
    seen_gop = false;
    seq_count = 0;
    firstgoppos = 0;
    prevgoppos = 0;

    fps = 29.97;

    lastKey = 0;

    positionMapType = MARK_UNSET;

    do_ac3_passthru = false;

    memset(&params, 0, sizeof(AVFormatParameters));
    memset(prvpkt, 0, 3);

    if (print_verbose_messages & VB_LIBAV)
        av_log_set_level(AV_LOG_DEBUG);
    else
        av_log_set_level(AV_LOG_ERROR);
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
    delete d;
}

void AvFormatDecoder::SeekReset(long long, int skipFrames, bool doflush)
{
    lastapts = 0;
    lastvpts = 0;

    av_read_frame_flush(ic);
    
    d->ResetMPEG2();

    ic->pb.pos = ringBuffer->GetTotalReadPosition();
    ic->pb.buf_ptr = ic->pb.buffer;
    ic->pb.buf_end = ic->pb.buffer;

    if (doflush)
    {
        for (int i = 0; i < ic->nb_streams; i++)
        {
            AVCodecContext *enc = &ic->streams[i]->codec;
            if (enc->codec)
                avcodec_flush_buffers(enc);
        }

        VideoFrame *buffer;
        for (buffer = inUseBuffers.first(); buffer; 
             buffer = inUseBuffers.next())
        {
            m_parent->DiscardVideoFrame(buffer);
        }

        inUseBuffers.clear();
    }

    while (storedPackets.count() > 0)
    {
        AVPacket *pkt = storedPackets.first();
        storedPackets.removeFirst();
        av_free_packet(pkt);
        delete pkt;
    }

    prevgoppos = 0;
    gopset = false;

    while (skipFrames > 0)
    {
        GetFrame(0);
        if (ateof)
            break;
        skipFrames--;
    }
}

void AvFormatDecoder::Reset(void)
{
    SeekReset();
#if 0
    // mpegts.c already clears the stream, so this code results in
    // silence on live tv with mpeg-ts recordings.

    // Clear out the existing mpeg streams
    // so we can get a clean set from the 
    // new seek position.
    for (int i = ic->nb_streams - 1; i >= 0; i--)
    {
        AVStream *st = ic->streams[i];
        if (st->codec.codec_type == CODEC_TYPE_AUDIO)
        {
            if (st->codec.codec)
                avcodec_close(&st->codec);
            av_remove_stream(ic, st->id);
        }
    }
#endif

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

static int avf_write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *h = (URLContext *)opaque;
    return url_write(h, buf, buf_size);
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

static QMap<void*, enum PixelFormat> _PixelFormatsMap;

/*
static enum PixelFormat getFormat(struct AVCodecContext *cc,
                                  const enum PixelFormat *pixfmts)
{
    if (_PixelFormatsMap.end() != _PixelFormatsMap.find(cc))
    {
        return _PixelFormatsMap[cc];
    }

    VERBOSE(VB_IMPORTANT, "Pixel format not set, returning pixfmts[0]");
    return pixfmts[0];
}
*/

void AvFormatDecoder::SetPixelFormat(const int pixFormat)
{
    PixelFormat pix = (PixelFormat)pixFormat;
    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *cc = &ic->streams[i]->codec;
        if (CODEC_TYPE_VIDEO == cc->codec_type)
            _PixelFormatsMap[cc]=pix;
    }
}

extern "C" void HandleStreamChange(void* data) {
    AvFormatDecoder* decoder = (AvFormatDecoder*) data;
    int cnt = decoder->ic->nb_streams;
    VERBOSE(VB_IMPORTANT, QString("streams_changed() -- stream count %1").arg(cnt));
    decoder->ScanStreams(false);
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
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: Couldn't decode file: \"%1\".").arg(filename));
        return -1;
    }

    fmt->flags |= AVFMT_NOFILE;

    ic = av_alloc_format_context();
    if (!ic)
    {
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: Couldn't allocate format context."));
        return -1;
    }

    InitByteContext();

    int err = av_open_input_file(&ic, filename, fmt, 0, &params);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: avformat error (%1) on av_open_input_file call.")
                                      .arg(err));
        return -1;
    }

    int ret = av_find_stream_info(ic);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder could not find codec parameters for \"%1\".")
                                      .arg(filename));
        av_close_input_file(ic);
        ic = NULL;
        return -1;
    }
    ic->streams_changed = HandleStreamChange;
    ic->stream_change_data = this;

    fmt->flags &= ~AVFMT_NOFILE;

    bitrate = 0;
    fps = 0;

    ret = ScanStreams(novideo);
    if (-1 == ret)
        return ret;

    autoSelectAudioTrack();

    ringBuffer->CalcReadAheadThresh(bitrate);

    if ((m_playbackinfo) || livetv || watchingrecording)
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
        // set the gop interval to 15 frames.  if we guess wrong, the
        // auto detection will change it.
        keyframedist = 15;
        positionMapType = MARK_GOP_START;
    }

    //if (livetv || watchingrecording)
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

int AvFormatDecoder::ScanStreams(bool novideo)
{
    int scanerror = 0;
    bitrate = 0;
    fps = 0;

    audioStreams.clear();
    do_ac3_passthru = false;
    audio_sample_size = -1;
    audio_sampling_rate = -1;
    audio_channels = -1;
    audio_check_1st = 2;
    audio_sampling_rate_2nd = -1;
    audio_channels_2nd = -1;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        VERBOSE(VB_PLAYBACK, "AVFD");
        VERBOSE(VB_PLAYBACK, QString("AVFD: Opening Stream #%1: codec id %2").
                arg(i).arg(enc->codec_id));

        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                assert(enc->codec_id);

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

                enc->error_resilience = FF_ER_COMPLIANT;
                enc->workaround_bugs = FF_BUG_AUTODETECT;
                enc->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
                enc->idct_algo = FF_IDCT_AUTO;
                enc->debug = 0;
                enc->rate_emu = 0;
                enc->error_rate = 0;

                d->DestroyMPEG2();
                if (!novideo && (enc->codec_id == CODEC_ID_MPEG1VIDEO ||
                    enc->codec_id == CODEC_ID_MPEG2VIDEO))
                {
#ifdef USING_XVMC
                    if (gContext->GetNumSetting("UseXVMC", 1))
                    {
                        enc->codec_id = CODEC_ID_MPEG2VIDEO_XVMC;
                        //enc->get_format = getFormat;
                    }
#endif
#ifdef USING_XVMC_VLD
                    if (gContext->GetNumSetting("UseXvMcVld", 1))
                        enc->codec_id = CODEC_ID_MPEG2VIDEO_XVMC_VLD;
#endif
                    // Only use libmpeg2 when not using XvMC
                    if (enc->codec_id == CODEC_ID_MPEG1VIDEO ||
                        enc->codec_id == CODEC_ID_MPEG2VIDEO)
                        d->InitMPEG2();
                }

                AVCodec *codec = avcodec_find_decoder(enc->codec_id);

                if (codec && (codec->id == CODEC_ID_MPEG2VIDEO_XVMC ||
                              codec->id == CODEC_ID_MPEG2VIDEO_XVMC_VLD))
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
                else if (codec && codec->capabilities & CODEC_CAP_DR1 &&
                    !(enc->width % 16))
                {
                    enc->flags |= CODEC_FLAG_EMU_EDGE;
                    enc->draw_horiz_band = NULL;
                    enc->get_buffer = get_avf_buffer;
                    enc->release_buffer = release_avf_buffer;
                    enc->opaque = (void *)this;
                    directrendering = true;
                }

                int align_width = enc->width;
                int align_height = enc->height;

                if (directrendering)
                    avcodec_align_dimensions(enc, &align_width, &align_height);

                m_parent->SetVideoParams(align_width, align_height, fps,
                                         keyframedist, aspect_ratio, kScan_Detect);
                break;
            }
            case CODEC_TYPE_AUDIO:
            {
                assert(enc->codec_id);
                if (enc->channels > 2)
                    enc->channels = 2;
                bitrate += enc->bit_rate;
                break;
            }
            case CODEC_TYPE_DATA:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, QString("AvFormatDecoder: data codec, ignoring (%1).")
                                             .arg(enc->codec_type));
                break;
            }
            default:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, QString("AvFormatDecoder: Unknown codec type (%1).")
                                             .arg(enc->codec_type));
                break;
            }
        }

        if (enc->codec_type != CODEC_TYPE_AUDIO && 
            enc->codec_type != CODEC_TYPE_VIDEO)
            continue;

        VERBOSE(VB_PLAYBACK, QString("AVFD: Looking for decoder for %1")
                                     .arg(enc->codec_id));
        AVCodec *codec = avcodec_find_decoder(enc->codec_id);
        if (!codec)
        {
            VERBOSE(VB_IMPORTANT, 
                    QString("AvFormatDecoder: Could not find decoder for "
                            "codec (%1), ignoring.")
                           .arg(enc->codec_id));
            continue;
        }

        if (enc->codec && enc->codec_type != CODEC_TYPE_VIDEO) 
        {
            VERBOSE(VB_IMPORTANT, QString("Codec already open, closing first"));
            avcodec_close(enc);
        }
        else if (enc->codec)
            continue;

        int open_val = avcodec_open(enc, codec);
        if (open_val < 0)
        {
            VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: Could not "
                    "open codec aborting. reason %1").arg(open_val));
            av_close_input_file(ic);
            ic = NULL;
            scanerror = -1;
            continue;
        }

        if (enc->codec_type == CODEC_TYPE_AUDIO)
        {
            audioStreams.push_back( i );
            VERBOSE(VB_AUDIO, QString("Stream #%1 (audio track #%2) is an "
                    "audio stream with %3 channels.")
                    .arg(i).arg(audioStreams.size() - 1).arg(enc->channels));
        }
    }

    if (bitrate > 0)
        bitrate /= 1000;

    // Select a new track at the next opportunity.
    currentAudioTrack = -1;
    return scanerror;
}

bool AvFormatDecoder::CheckVideoParams(int width, int height)
{
    if (width == current_width && height == current_height)
        return false;

    VERBOSE(VB_ALL, 
            QString("AvFormatDecoder: Video has changed from %1x%2 to %3x%4.")
               .arg(current_width).arg(current_height).arg(width).arg(height));

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = &ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                AVCodec *codec = enc->codec;
                if (!codec) {
                    VERBOSE(VB_IMPORTANT, QString("codec for stream %1 is null").arg(i));
                    break;
                }
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

void AvFormatDecoder::CheckAudioParams(int freq, int channels, bool safe)
{
    if (freq <= 0 || channels <= 0)
        return;

    if (safe || audio_check_1st == 2)
    {
        if (freq == audio_sampling_rate && channels == audio_channels)
            return;
        audio_check_1st = 1;
        audio_sampling_rate_2nd = freq;
        audio_channels_2nd = channels;
        if (safe == false)
            return;
    }
    else
    {
        if (freq != audio_sampling_rate_2nd || channels != audio_channels_2nd ||
            (freq == audio_sampling_rate && channels == audio_channels))
        {
            audio_sampling_rate_2nd = -1;
            audio_channels_2nd = -1;
            audio_check_1st = 2;
            return;
        }

        if (audio_check_1st == 1)
        {
            audio_check_1st = 0;
            return;
        }
    }

    audio_check_1st = 2;

    if (audio_channels != -1)
        VERBOSE(VB_AUDIO, QString("Audio format changed from %1 channels,"
                " %2hz to %3 channels %4hz").arg(audio_channels)
                .arg(audio_sampling_rate).arg(channels).arg(freq));

    AVCodecContext *enc = &ic->streams[wantedAudioStream]->codec;
    AVCodec *codec = enc->codec;
    if (codec)
        avcodec_close(enc);
    codec = avcodec_find_decoder(enc->codec_id);
    avcodec_open(enc, codec);

    if (do_ac3_passthru)
    {
        // An AC3 stream looks like a 48KHz 2ch audio stream to
        // the sound card
        audio_sample_size = 4;
        audio_sampling_rate = 48000;
        audio_channels = 2;
    }
    else
    {
        audio_sample_size = channels * 2;
        audio_sampling_rate = freq;
        audio_channels = channels;
    }

    m_parent->SetAudioParams(16, audio_channels, audio_sampling_rate);
    m_parent->ReinitAudio();
    return;
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
    if (!src)
        return;

    (void)offset;
    (void)type;

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
            VERBOSE(VB_PLAYBACK, QString("HandleGopStart: gopset not set, "
                                         "syncing positionMap"));
            SyncPositionMap();
            if (tempKeyFrameDist > 0)
            {
                gopset = true;
                keyframedist = tempKeyFrameDist;

                if ((keyframedist == 15) ||
                    (keyframedist == 12))
                    positionMapType = MARK_GOP_START;
                else
                    positionMapType = MARK_GOP_BYFRAME;

                VERBOSE(VB_PLAYBACK, QString("Stream initial keyframedist: %1.").arg(keyframedist));
                m_parent->SetKeyframeDistance(keyframedist);
            }
        }
        else
        {
            if (keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
            {
                VERBOSE(VB_PLAYBACK, QString ("KeyFrameDistance has changed to %1 from %2.")
                                             .arg(tempKeyFrameDist).arg(keyframedist));

                keyframedist = tempKeyFrameDist;

                if ((keyframedist == 15) ||
                    (keyframedist == 12))
                    positionMapType = MARK_GOP_START;
                else
                    positionMapType = MARK_GOP_BYFRAME;

                m_parent->SetKeyframeDistance(keyframedist);

                // also reset length
                long long index = m_positionMap[m_positionMap.size() - 1].index;
                long long totframes = index * keyframedist;
                int length = (int)((totframes * 1.0) / fps);

                VERBOSE(VB_PLAYBACK, QString("Setting(2) length to: %1, "
                                             "total frames %2")
                                             .arg(length).arg((int)totframes));
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

        //cerr << "framesRead: " << framesRead << " last_frame: " << last_frame
        //    << " keyframedist: " << keyframedist << endl;

        // if we don't have an entry, fill it in with what we've just parsed
        if (framesRead > last_frame && keyframedist > 0)
        {
            long long startpos = pkt->startpos;
            VERBOSE(VB_PLAYBACK, QString("positionMap[ %1 ] == %2.").arg(prevgoppos / keyframedist)
                                         .arg((int)startpos));

            // Grow positionMap vector several entries at a time
            if (m_positionMap.capacity() == m_positionMap.size())
                m_positionMap.reserve(m_positionMap.size() + 60);
            PosMapEntry entry = {prevgoppos / keyframedist,
                                 prevgoppos, startpos};
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
    //unsigned char *bufend = pkt->data + pkt->size;
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
                    //int size = 8;
                    //if (bufptr + size > bufend)
                    //    return;
                    unsigned char *test = bufptr;
                    /* XXX: Doesn't work properly.
                    if (test[size - 1] & 0x40)
                        size += 64;
                    if (bufptr + size > bufend)
                        return;
                    if (test[size - 1] & 0x80)
                        size += 64;
                    if ((bufptr + size + 4) > bufend)
                        return;
                    test = bufptr + size;
                    if (test[0] != 0x0 || test[1] != 0x0 || test[2] != 0x1) {
                        cerr << "avfd: sequence header size mismatch" << endl;
                        continue;
                    }

                    test = bufptr;*/
                    int width = (test[0] << 4) | (test[1] >> 4);
                    int height = ((test[1] & 0xff) << 8) | test[2];

                    int aspectratioinfo = (test[3] >> 4);

                    float aspect = GetMpegAspect(context, aspectratioinfo,
                                                 width, height);

                    if (width < 2500 && height < 2000 &&
                        (CheckVideoParams(width, height) ||
                         aspect != current_aspect))
                    {
                        int align_width = width;
                        int align_height = height;

                        fps = (double)context->frame_rate / 
                                      context->frame_rate_base;

                        if (directrendering)
                            avcodec_align_dimensions(context, &align_width, 
                                                     &align_height);

                        m_parent->SetVideoParams(align_width,
                                                 align_height, fps,
                                                 keyframedist, aspect, 
                                                 kScan_Detect, true);
                        current_width = width;
                        current_height = height;
                        current_aspect = aspect;

                        if (d->mpeg2dec)
                            d->ResetMPEG2();

                        gopset = false;
                        prevgoppos = 0;
                        lastapts = lastvpts = 0;
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

void AvFormatDecoder::incCurrentAudioTrack()
{
    if (audioStreams.size())
    {
        int tempTrackNo = currentAudioTrack;
        ++tempTrackNo;
        if (tempTrackNo > (int)(audioStreams.size() - 1))
            tempTrackNo = 0;

        currentAudioTrack = tempTrackNo;
        wantedAudioStream = audioStreams[currentAudioTrack];

        AVCodecContext *e = &ic->streams[wantedAudioStream]->codec;
        SetupAudioStream();
        CheckAudioParams(e->sample_rate, e->channels, true);
    }
}

void AvFormatDecoder::decCurrentAudioTrack()
{
    if (audioStreams.size())
    {
        int tempTrackNo = currentAudioTrack;
        --tempTrackNo;
        if (tempTrackNo < 0)
            tempTrackNo = audioStreams.size() - 1;

        currentAudioTrack = tempTrackNo;
        wantedAudioStream = audioStreams[currentAudioTrack];

        AVCodecContext *e = &ic->streams[wantedAudioStream]->codec;
        SetupAudioStream();
        CheckAudioParams(e->sample_rate, e->channels, true);
    }
}

bool AvFormatDecoder::setCurrentAudioTrack(int trackNo)
{
    if (trackNo < 0)
        trackNo = -1;
    else if (trackNo >= (int)audioStreams.size())
        return false;

    currentAudioTrack = trackNo;
    if (currentAudioTrack < 0)
        return false;

    wantedAudioStream = audioStreams[currentAudioTrack];

    AVCodecContext *e = &ic->streams[wantedAudioStream]->codec;
    SetupAudioStream();
    CheckAudioParams(e->sample_rate, e->channels, true);
    return true;
}

//
// This function will select the best audio track
// available usgin the following rules:
//
// 1. The fist AC3 track found will be select,
// 2. If no AC3 is found then the audio track with
// the most number of channels is selected. 
//
// This code has no awareness to language preferences
// although I don't think it would be too hard to 
// add.
//
bool AvFormatDecoder::autoSelectAudioTrack()
{
    if (!audioStreams.size())
        return false;

    int minChannels = 1;
    int maxTracks = (audioStreams.size() - 1);
    int track;
    if (do_ac3_passthru)
        minChannels = 2;

    int selectedTrack = -1;
    int selectedChannels = -1;
    
    while ((selectedTrack == -1) && (minChannels >= 0))
    {
        for (track = 0; track <= maxTracks; track++)
        {
            int tempStream = audioStreams[track];
            AVCodecContext *e = &ic->streams[tempStream]->codec;

            if (e->channels > minChannels)
            {
                //if we find an AC3 codec then we select it 
                //as the preferred codec.
                if (e->codec_id == CODEC_ID_AC3)
                {
                    selectedTrack = track;
                    break;
                }

                if (e->channels > selectedChannels)
                {
                    // this is a candidate with more channels
                    // than the previous, or there was no previous
                    // so select it.
                    selectedChannels = e->channels;
                    selectedTrack = track;
                }
            }
        }
        if (selectedTrack > maxTracks)
        {
            minChannels--;
        }
    }

    if (selectedTrack > maxTracks)
    {
        VERBOSE(VB_AUDIO,
                QString("No suitable audio track exists."));
        return false;
    }

    currentAudioTrack = selectedTrack;
    wantedAudioStream = audioStreams[currentAudioTrack];
     
    AVCodecContext *e = &ic->streams[wantedAudioStream]->codec;
    if (e->codec_id == CODEC_ID_AC3)
    {
        VERBOSE(VB_AUDIO, 
                QString("Auto-selecting AC3 audio track (stream #%2).")
                .arg(wantedAudioStream)); 
    }
    else
    {
        VERBOSE(VB_AUDIO, 
                QString("Auto-selecting audio track #%1 (stream #%2).")
                .arg(selectedTrack + 1).arg(wantedAudioStream));
        VERBOSE(VB_AUDIO, 
                QString("It has %1 channels and we needed at least %2")
                .arg(selectedChannels).arg(do_ac3_passthru ? 2 : 1));
    }
    SetupAudioStream();
    CheckAudioParams(e->sample_rate, e->channels, true);
    return true;
}

void AvFormatDecoder::SetupAudioStream(void)
{
    if (wantedAudioStream >= ic->nb_streams || currentAudioTrack < 0)
        return;

    AVStream *curstream = ic->streams[wantedAudioStream];
    if (curstream == NULL)
        return;

    VERBOSE(VB_AUDIO, QString("Initializing audio parms from audio track #%1.")
            .arg(currentAudioTrack));

    m_parent->SetEffDsp(curstream->codec.sample_rate * 100);

    do_ac3_passthru = curstream->codec.codec_id == CODEC_ID_AC3 &&
                      !transcoding &&
                      gContext->GetNumSetting("AC3PassThru", false);
}

bool AvFormatDecoder::GetFrame(int onlyvideo)
{
    AVPacket *pkt = NULL;
    int len, ret = 0;
    unsigned char *ptr;
    int data_size = 0;
    long long pts;
    bool firstloop = false, have_err = false;

    gotvideo = false;

    frame_decoded = 0;

    bool allowedquit = false;
    bool storevideoframes = false;

    if (currentAudioTrack == -1 || 
        currentAudioTrack >= (int)audioStreams.size())
    {
        autoSelectAudioTrack();
    }

    bool skipaudio = (lastvpts == 0);

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
                return false;
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
        if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
            pts = pkt->dts / (AV_TIME_BASE / 1000);

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
                context->codec_id == CODEC_ID_MPEG2VIDEO_XVMC_VLD)
            {
                MpegPreProcessPkt(curstream, pkt);
            }
            else
            {
                if (pkt->flags & PKT_FLAG_KEY)
                {
                    HandleGopStart(pkt);
                    seen_gop = true;
                }
                else
                {
                    seq_count++;
                    if (!seen_gop && seq_count > 1)
                    {
                        HandleGopStart(pkt);
                    }
                }
            }
        }

        if (!curstream->codec.codec)
        {
            VERBOSE(VB_PLAYBACK, QString("No codec for stream index %1").arg(pkt->stream_index));
            av_free_packet(pkt);
            continue;
        }

        firstloop = true;
        have_err = false;

        while (!have_err && len > 0)
        {
            switch (curstream->codec.codec_type)
            {
                case CODEC_TYPE_AUDIO:
                {
                    if (firstloop && pkt->pts != (int64_t)AV_NOPTS_VALUE)
                        lastapts = pkt->pts / (AV_TIME_BASE / 1000);

                    if (onlyvideo != 0 ||
                        (pkt->stream_index != wantedAudioStream))
                    {
                        ptr += pkt->size;
                        len -= pkt->size;
                        continue;
                    }

                    if (skipaudio)
                    {
                        if (lastapts < lastvpts - 30 || lastvpts == 0)
                        {
                            ptr += pkt->size;
                            len -= pkt->size;
                            continue;
                        }
                        else
                            skipaudio = false;
                    }
                        
                    if (do_ac3_passthru)
                    {
                        data_size = pkt->size;
                        ret = EncodeAC3Frame(ptr, len, audioSamples,
                                             data_size);
                    }
                    else
                    {
                        ret = avcodec_decode_audio(&curstream->codec,
                                                   audioSamples, &data_size,
                                                   ptr, len);
                    }

                    ptr += ret;
                    len -= ret;

                    if (data_size <= 0)
                        continue;

                    if (!do_ac3_passthru)
                        CheckAudioParams(curstream->codec.sample_rate,
                                         curstream->codec.channels, false);

                    long long temppts = lastapts;

                    // calc for next frame
                    lastapts += (long long)((double)(data_size * 1000) /
                                (curstream->codec.channels * 2) / curstream->codec.sample_rate);

                    m_parent->AddAudioData((char *)audioSamples, data_size, temppts);

                    break;
                }
                case CODEC_TYPE_VIDEO:
                {
                    if (onlyvideo < 0)
                    {
                        framesPlayed++;
                        ptr += pkt->size;
                        len -= pkt->size;
                        continue;
                    }

                    AVCodecContext *context = &(curstream->codec);
                    AVFrame mpa_pic;

                    int gotpicture = 0;

                    pthread_mutex_lock(&avcodeclock);
                    if (d->mpeg2dec)
                        ret = d->DecodeMPEG2Video(context, &mpa_pic,
                                                  &gotpicture, ptr, len);
                    else
                        ret = avcodec_decode_video(context, &mpa_pic,
                                                   &gotpicture, ptr, len);
                    pthread_mutex_unlock(&avcodeclock);

                    if (ret < 0)
                    {
                        cerr << "decoding error\n";
                        have_err = true;
                        continue;
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
 
                        picframe = m_parent->GetNextVideoFrame();

                        tmppicture.data[0] = picframe->buf;
                        tmppicture.data[1] = tmppicture.data[0] + 
                                        picframe->width * picframe->height;
                        tmppicture.data[2] = tmppicture.data[1] + 
                                        picframe->width * picframe->height / 4;

                        tmppicture.linesize[0] = picframe->width;
                        tmppicture.linesize[1] = picframe->width / 2;
                        tmppicture.linesize[2] = picframe->width / 2;

                        img_convert(&tmppicture, PIX_FMT_YUV420P, 
                                    (AVPicture *)&mpa_pic,
                                    context->pix_fmt,
                                    context->width,
                                    context->height);
                    }

                    long long temppts = pts;

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

                    picframe->interlaced_frame = mpa_pic.interlaced_frame;
                    picframe->top_field_first = mpa_pic.top_field_first;

                    picframe->frameNumber = framesPlayed;
                    m_parent->ReleaseNextVideoFrame(picframe, temppts);
                    if (directrendering)
                        inUseBuffers.removeRef(picframe);
                    gotvideo = 1;
                    framesPlayed++;

                    lastvpts = temppts;
                    break;
                }
                default:
                    cerr << "error decoding - " << curstream->codec.codec_type
                         << endl;
                    have_err = true;
                    break;
            }

            if (!have_err)
            {
                ptr += ret;
                len -= ret;
                frame_decoded = 1;
                firstloop = false;
            }
        }

        av_free_packet(pkt);
    }

    if (pkt)
        delete pkt;

    return true;
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

