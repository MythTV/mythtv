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
#include "iso639.h"

#ifdef USING_XVMC
#include "videoout_xv.h"
extern "C" {
#include "libavcodec/xvmc_render.h"
}
#endif
extern "C" {
#include "libavcodec/liba52/a52.h"
#include "../libmythmpeg2/mpeg2.h"
}

#define MAX_AC3_FRAME_SIZE 6144

extern pthread_mutex_t avcodeclock;

void align_dimensions(AVCodecContext *avctx, int &width, int &height)
{
    // minimum buffer alignment
    avcodec_align_dimensions(avctx, &width, &height);
    // minimum MPEG alignment
    width  = (width  + 15) & (~0xf);
    height = (height + 15) & (~0xf);
}

class AvFormatDecoderPrivate
{
  public:
    AvFormatDecoderPrivate() : mpeg2dec(NULL) { ; }
   ~AvFormatDecoderPrivate() { DestroyMPEG2(); }
    
    bool InitMPEG2();
    bool HasMPEG2Dec() const { return (bool)(mpeg2dec); }

    void DestroyMPEG2();
    void ResetMPEG2();
    int DecodeMPEG2Video(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr, uint8_t *buf, int buf_size);

  private:
    mpeg2dec_t *mpeg2dec;
};

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
                    picture->top_field_first = !!(info->display_picture->flags &
                                                  PIC_FLAG_TOP_FIELD_FIRST);
                    picture->interlaced_frame = !(info->display_picture->flags &
                                                  PIC_FLAG_PROGRESSIVE_FRAME);
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

AvFormatDecoder::AvFormatDecoder(NuppelVideoPlayer *parent, ProgramInfo *pginfo,
                                 bool use_null_videoout)
    : DecoderBase(parent, pginfo),
      d(new AvFormatDecoderPrivate()), ic(NULL),
      frame_decoded(0), directrendering(false), drawband(false), bitrate(0),
      gopset(false), seen_gop(false), seq_count(0), firstgoppos(0),
      prevgoppos(0), gotvideo(false), lastvpts(0), lastapts(0),
      using_null_videoout(use_null_videoout), video_codec_id(kCodec_NONE),
      maxkeyframedist(-1), 
      // Audio
      audioSamples(new short int[AVCODEC_MAX_AUDIO_FRAME_SIZE]),
      audio_sample_size(-1), audio_sampling_rate(-1), audio_channels(-1),
      do_ac3_passthru(false), wantedAudioStream(-1),
      audio_check_1st(2), audio_sampling_rate_2nd(0), audio_channels_2nd(0),
      wantedSubtitleStream(-1)
{
    bzero(&params, sizeof(AVFormatParameters));
    bzero(prvpkt, 3 * sizeof(char));
    bzero(audioSamples, AVCODEC_MAX_AUDIO_FRAME_SIZE * sizeof(short int));

    bool debug = (bool)(print_verbose_messages & VB_LIBAV);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
}

AvFormatDecoder::~AvFormatDecoder()
{
    CloseContext();
    delete d;
    if (audioSamples)
        delete [] audioSamples;
}

void AvFormatDecoder::CloseContext()
{
    if (ic)
    {
        for (int i = 0; i < ic->nb_streams; i++)
        {
            AVStream *st = ic->streams[i];
            if (st->codec->codec)
                avcodec_close(st->codec);
        }

        ic->iformat->flags |= AVFMT_NOFILE;

        av_free(ic->pb.buffer);
        av_close_input_file(ic);
        ic = NULL;
    }
    d->DestroyMPEG2();
}

static int64_t lsb3full(int64_t lsb, int64_t base_ts, int lsb_bits)
{
    int64_t mask = (lsb_bits < 64) ? (1LL<<lsb_bits)-1 : -1LL;
    return  ((lsb - base_ts)&mask);
}

bool AvFormatDecoder::DoRewind(long long desiredFrame, bool doflush)
{
    VERBOSE(VB_PLAYBACK, "AvFormatDecoder::DoRewind("
            <<desiredFrame<<", "<<( doflush ? "do" : "don't" )<<" flush)");

    if (recordingHasPositionMap || livetv)
        return DecoderBase::DoRewind(desiredFrame, doflush);

    // avformat-based seeking
    return DoFastForward(desiredFrame, doflush);
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame, bool doflush)
{
    VERBOSE(VB_PLAYBACK, "AvFormatDecoder::DoFastForward("
            <<desiredFrame<<", "<<( doflush ? "do" : "don't" )<<" flush)");

    if (recordingHasPositionMap || livetv)
        return DecoderBase::DoFastForward(desiredFrame, doflush);

    bool oldrawstate = getrawframes;
    getrawframes = false;

    AVStream *st = NULL;
    int i;
    for (i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st1 = ic->streams[i];
        if (st1 && st1->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }
    if (!st)
        return false;

    int64_t frameseekadjust = 0;
    AVCodecContext *context = st->codec;

    if (context->codec_id == CODEC_ID_MPEG1VIDEO ||
        context->codec_id == CODEC_ID_MPEG2VIDEO ||
        context->codec_id == CODEC_ID_MPEG2VIDEO_XVMC ||
        context->codec_id == CODEC_ID_MPEG2VIDEO_XVMC_VLD)
    {
        frameseekadjust = maxkeyframedist+1;
    }

    // convert framenumber to normalized timestamp
    long double diff = (max(desiredFrame - frameseekadjust, 0LL)) * AV_TIME_BASE;
    long long ts = (long long)( diff / fps );
    if (av_seek_frame(ic, -1, ts, AVSEEK_FLAG_BACKWARD) < 0)
    {
        VERBOSE(VB_IMPORTANT, "av_seek_frame(ic, -1, "
                <<ts<<", 0) -- error");
        return false;
    }

    int64_t adj_cur_dts = st->cur_dts;

    if(ic->start_time != (int64_t)AV_NOPTS_VALUE)
    {
        int64_t st1 = av_rescale(ic->start_time,
                                st->time_base.den,
                                AV_TIME_BASE * (int64_t)st->time_base.num);
        adj_cur_dts = lsb3full(adj_cur_dts, st1, st->pts_wrap_bits);
    }

    long long newts = av_rescale(adj_cur_dts,
                            (int64_t)AV_TIME_BASE * (int64_t)st->time_base.num,
                            st->time_base.den);

    // convert current timestamp to frame number
    lastKey = (long long)((newts*(long double)fps)/AV_TIME_BASE);
    framesPlayed = lastKey;
    framesRead = lastKey;

    int normalframes = desiredFrame - framesPlayed;

    SeekReset(lastKey, normalframes, doflush);

    if (doflush)
    {
        GetNVP()->SetFramesPlayed(framesPlayed + 1);
        GetNVP()->getVideoOutput()->SetFramesPlayed(framesPlayed + 1);
    }

    getrawframes = oldrawstate;

    return true;
}

void AvFormatDecoder::SeekReset(long long, int skipFrames, bool doflush)
{
    VERBOSE(VB_PLAYBACK, "AvFormatDecoder::SeekReset("
            <<skipFrames<<", "<<( doflush ? "do" : "don't" )<<" flush)");

    lastapts = 0;
    lastvpts = 0;

    av_read_frame_flush(ic);
    
    d->ResetMPEG2();

    // only reset the internal state if we're using our seeking, not libavformat's
    if (recordingHasPositionMap)
    {
        ic->pb.pos = ringBuffer->GetTotalReadPosition();
        ic->pb.buf_ptr = ic->pb.buffer;
        ic->pb.buf_end = ic->pb.buffer;
    }

    if (doflush)
    {
        VERBOSE(VB_PLAYBACK, "AvFormatDecoder::SeekReset() flushing");
        for (int i = 0; i < ic->nb_streams; i++)
        {
            AVCodecContext *enc = ic->streams[i]->codec;
            if (enc->codec)
                avcodec_flush_buffers(enc);
        }

        // TODO here we may need to wait for flushing to complete...

        GetNVP()->DiscardVideoFrames();
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
        GetNVP()->ReleaseNextVideoFrame();

        if (ateof)
            break;
        skipFrames--;
    }
}

void AvFormatDecoder::Reset(bool reset_video_data, bool seek_reset)
{
    VERBOSE(VB_PLAYBACK, QString("AvFormatDecoder::Reset(%1, %2)")
            .arg(reset_video_data).arg(seek_reset));
    if (seek_reset)
        SeekReset();

    if (reset_video_data)
    {
        m_positionMap.clear();
        framesPlayed = 0;
        framesRead = 0;
        seen_gop = false;
        seq_count = 0;
    }
}

void AvFormatDecoder::Reset()
{
    DecoderBase::Reset();

    // mpeg ts reset
    if (QString("mpegts") == ic->iformat->name)
    {
        AVInputFormat *fmt = (AVInputFormat*) av_mallocz(sizeof(AVInputFormat));
        memcpy(fmt, ic->iformat, sizeof(AVInputFormat));
        fmt->flags |= AVFMT_NOFILE;

        CloseContext();
        ic = av_alloc_format_context();
        if (!ic)
        {
            VERBOSE(VB_IMPORTANT, "AvFormatDecoder: Error, could not "
                    "allocate format context.");
            av_free(fmt);
            errored = true;
            return;
        }

        InitByteContext();
        ic->streams_changed = HandleStreamChange;
        ic->stream_change_data = this;

        char *filename = (char *)(ringBuffer->GetFilename().ascii());
        int err = av_open_input_file(&ic, filename, fmt, 0, &params);
        if (err < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("AvFormatDecoder: avformat error (%1) "
                            "on av_open_input_file call.").arg(err));
            av_free(fmt);
            errored = true;
            return;
        }

        fmt->flags &= ~AVFMT_NOFILE;
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

    if (whence == SEEK_END)
        return dec->getRingBuf()->GetRealFileSize() + offset;

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

static offset_t avf_seek_packet(void *opaque, int64_t offset, int whence)
{
    URLContext *h = (URLContext *)opaque;
    return url_seek(h, offset, whence);
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

extern "C" void HandleStreamChange(void* data) {
    AvFormatDecoder* decoder = (AvFormatDecoder*) data;
    int cnt = decoder->ic->nb_streams;
    VERBOSE(VB_PLAYBACK, "streams_changed "<<data<<" -- stream count "<<cnt);
    pthread_mutex_lock(&avcodeclock);
    decoder->SeekReset();
    decoder->ScanStreams(false);
    pthread_mutex_unlock(&avcodeclock);
}

/** \fn AvFormatDecoder::OpenFile(RingBuffer*, bool, char[2048])
 *  OpenFile opens a ringbuffer for playback.
 *
 *  OpenFile deletes any existing context then use testbuf to
 *  guess at the stream type. It then calls ScanStreams to find
 *  any valid streams to decode. If possible a position map is
 *  also built for quick skipping.
 *
 *  /param rbuffer pointer to a valid ringuffer.
 *  /param novideo if true then no video is sought in ScanSreams.
 *  /param _testbuf this paramater is not used by AvFormatDecoder.
 */
int AvFormatDecoder::OpenFile(RingBuffer *rbuffer, bool novideo, char testbuf[2048])
{
    CloseContext();

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
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: Couldn't decode "
                                      "file: \"%1\".").arg(filename));
        return -1;
    }

    fmt->flags |= AVFMT_NOFILE;

    ic = av_alloc_format_context();
    if (!ic)
    {
        VERBOSE(VB_IMPORTANT, "AvFormatDecoder: Error, could not "
                "allocate format context.");
        return -1;
    }

    InitByteContext();

    int err = av_open_input_file(&ic, filename, fmt, 0, &params);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: avformat error (%1) "
                                      "on av_open_input_file call.").arg(err));
        return -1;
    }

    int ret = av_find_stream_info(ic);
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("AvFormatDecoder: could not find codec "
                                      "parameters for \"%1\".").arg(filename));
        av_close_input_file(ic);
        ic = NULL;
        return -1;
    }
    ic->streams_changed = HandleStreamChange;
    ic->stream_change_data = this;

    fmt->flags &= ~AVFMT_NOFILE;

    //struct timeval one, two, res;
    //gettimeofday(&one, NULL);
    av_estimate_timings(ic);
    //gettimeofday(&two, NULL);

    //timersub(&two, &one, &res);
    //printf("estimate: %d.%06d\n", (int)res.tv_sec, (int)res.tv_usec);

    bitrate = 0;
    fps = 0;

    ret = ScanStreams(novideo);
    if (-1 == ret)
        return ret;

    autoSelectAudioTrack();
    autoSelectSubtitleTrack();

    ringBuffer->CalcReadAheadThresh(bitrate);

    if (!recordingHasPositionMap)
    {
        if ((m_playbackinfo) || livetv || watchingrecording)
        {
            recordingHasPositionMap |= SyncPositionMap();
            if (recordingHasPositionMap && !livetv && !watchingrecording)
            {
                hasFullPositionMap = true;
                gopset = true;
            }
        }
    }

    if (!recordingHasPositionMap)
    {
        VERBOSE(VB_PLAYBACK, "recording has no position -- using libavformat");
        int64_t dur = ic->duration / (int64_t)AV_TIME_BASE;

        if (dur > 0)
        {
            GetNVP()->SetFileLength((int)(dur), (int)(dur * fps));
        }
        else
        {
            // the pvr-250 seems to overreport the bitrate by * 2
            float bytespersec = (float)bitrate / 8 / 2;
            float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
            GetNVP()->SetFileLength((int)(secs), (int)(secs * fps));
        }

        // we will not see a position map from db or remote encoder,
        // set the gop interval to 15 frames.  if we guess wrong, the
        // auto detection will change it.
        keyframedist = 15;
        positionMapType = MARK_GOP_START;

        if (!strcmp(fmt->name, "avi"))
        {
            // avi keyframes are too irregular
            keyframedist = 1;
            positionMapType = MARK_GOP_BYFRAME;
        }
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

    VERBOSE(VB_PLAYBACK,
            QString("AvFormatDecoder: Successfully opened decoder for file: "
                    "\"%1\". novideo(%2)").arg(filename).arg(novideo));

    return recordingHasPositionMap;
}

void AvFormatDecoder::InitVideoCodec(AVCodecContext *enc)
{
    fps = 1 / av_q2d(enc->time_base);
    // Some formats report fps waaay too high. (wrong time_base)
    if (fps > 100)
        fps /= 100;

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

    enc->opaque = NULL;
    enc->get_buffer = NULL;
    enc->release_buffer = NULL;
    enc->draw_horiz_band = NULL;
    enc->slice_flags = 0;

    enc->error_resilience = FF_ER_COMPLIANT;
    enc->workaround_bugs = FF_BUG_AUTODETECT;
    enc->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    enc->idct_algo = FF_IDCT_AUTO;
    enc->debug = 0;
    enc->rate_emu = 0;
    enc->error_rate = 0;

    AVCodec *codec = avcodec_find_decoder(enc->codec_id);    

    if (!gContext->GetNumSetting("DecodeExtraAudio", 0))
        SetLowBuffers(false);

    if (codec && (codec->id == CODEC_ID_MPEG2VIDEO_XVMC ||
                  codec->id == CODEC_ID_MPEG2VIDEO_XVMC_VLD))
    {
        enc->flags |= CODEC_FLAG_EMU_EDGE;
        enc->opaque = (void *)this;
        enc->get_buffer = get_avf_buffer_xvmc;
        enc->release_buffer = release_avf_buffer_xvmc;
        enc->draw_horiz_band = render_slice_xvmc;
        enc->slice_flags = SLICE_FLAG_CODED_ORDER |
            SLICE_FLAG_ALLOW_FIELD;
        directrendering = true;
    }
    else if (codec && codec->capabilities & CODEC_CAP_DR1 &&
             !(enc->width % 16))
    {
        enc->flags |= CODEC_FLAG_EMU_EDGE;
        enc->opaque = (void *)this;
        enc->get_buffer = get_avf_buffer;
        enc->release_buffer = release_avf_buffer;
        enc->draw_horiz_band = NULL;
        directrendering = true;
    }

    int align_width = enc->width;
    int align_height = enc->height;

    align_dimensions(enc, align_width, align_height);

    if (align_width == 0 && align_height == 0)
    {
        align_width = 640;
        align_height = 480;
        fps = 29.97;
        aspect_ratio = 4.0 / 3;
    }

    GetNVP()->SetVideoParams(align_width, align_height, fps,
                             keyframedist, aspect_ratio, kScan_Detect);
}

#ifdef USING_XVMC
static int xvmc_stream_type(int codec_id)
{
    switch (codec_id)
    {
        case CODEC_ID_MPEG1VIDEO:
            return 1;
        case CODEC_ID_MPEG2VIDEO:
        case CODEC_ID_MPEG2VIDEO_XVMC:
        case CODEC_ID_MPEG2VIDEO_XVMC_VLD:
            return 2;
#if 0
// We don't support these yet.
        case CODEC_ID_H263:
            return 3;
        case CODEC_ID_MPEG4:
            return 4;
#endif
    }
    return 0;
}

static int xvmc_pixel_format(enum PixelFormat pix_fmt)
{
    (void) pix_fmt;
    int xvmc_chroma = XVMC_CHROMA_FORMAT_420;
#if 0
// We don't support other chromas yet
    if (PIX_FMT_YUV420P == pix_fmt)
        xvmc_chroma = XVMC_CHROMA_FORMAT_420;
    else if (PIX_FMT_YUV422P == pix_fmt)
        xvmc_chroma = XVMC_CHROMA_FORMAT_422;
    else if (PIX_FMT_YUV420P == pix_fmt)
        xvmc_chroma = XVMC_CHROMA_FORMAT_444;
#endif
    return xvmc_chroma;
}
#endif // USING_XVMC

int AvFormatDecoder::ScanStreams(bool novideo)
{
    int scanerror = 0;
    bitrate = 0;
    fps = 0;

    audioStreams.clear();
    subtitleStreams.clear();
    do_ac3_passthru = false;
    audio_sample_size = -1;
    audio_sampling_rate = -1;
    audio_channels = -1;
    audio_check_1st = 2;
    audio_sampling_rate_2nd = -1;
    audio_channels_2nd = -1;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = ic->streams[i]->codec;
        VERBOSE(VB_PLAYBACK,
                QString("AVFD: Stream #%1, has id 0x%2 codec id %3, type %4 at 0x")
                .arg(i).arg((int)ic->streams[i]->id)
                .arg(codec_id_string(enc->codec_id))
                .arg(codec_type_string(enc->codec_type))
                <<((void*)ic->streams[i]));

        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                assert(enc->codec_id);
                bitrate += enc->bit_rate;
                if (novideo)
                    break;

                d->DestroyMPEG2();
#ifdef USING_XVMC
                if (!using_null_videoout && xvmc_stream_type(enc->codec_id))
                {
                    // HACK -- begin
                    // Force MPEG2 decoder on MPEG1 streams.
                    // Needed for broken transmitters which mark
                    // MPEG2 streams as MPEG1 streams, and should
                    // be harmless for unbroken ones.
                    if (CODEC_ID_MPEG1VIDEO == enc->codec_id)
                        enc->codec_id = CODEC_ID_MPEG2VIDEO;
                    // HACK -- end

                    MythCodecID mcid;
                    mcid = VideoOutputXv::GetBestSupportedCodec(
                        /* disp dim     */ enc->width, enc->height,
                        /* osd dim      */ /*enc->width*/ 0, /*enc->height*/ 0,
                        /* mpeg type    */ xvmc_stream_type(enc->codec_id),
                        /* xvmc pix fmt */ xvmc_pixel_format(enc->pix_fmt),
                        /* test surface */ kCodec_NORMAL_END > video_codec_id);
                    bool vcd, idct, mc;
                    enc->codec_id = myth2av_codecid(mcid, vcd, idct, mc);
                    video_codec_id = mcid;
                    if (kCodec_NORMAL_END < mcid && kCodec_STD_XVMC_END > mcid)
                    {
                        enc->pix_fmt = (idct) ?
                            PIX_FMT_XVMC_MPEG2_IDCT : PIX_FMT_XVMC_MPEG2_MC;
                    }
                }
                else
                    video_codec_id = kCodec_MPEG2; // default to MPEG2
#else
                video_codec_id = kCodec_MPEG2; // default to MPEG2
#endif // USING_XVMC
                if (enc->codec)
                {
                    VERBOSE(VB_IMPORTANT,
                            "AVFD: Warning, video codec "<<enc<<" "
                            <<"id("<<codec_id_string(enc->codec_id)<<") "
                            <<"type ("<<codec_type_string(enc->codec_type)<<") "
                            <<"already open.");
                }
                InitVideoCodec(enc);
                // Only use libmpeg2 when not using XvMC
                if (CODEC_ID_MPEG1VIDEO == enc->codec_id ||
                    CODEC_ID_MPEG2VIDEO == enc->codec_id)
                {
                    d->InitMPEG2();
                }
                break;
            }
            case CODEC_TYPE_AUDIO:
            {
                if (enc->codec)
                {
                    VERBOSE(VB_IMPORTANT,
                            "AVFD: Warning, audio codec "<<enc
                            <<" id("<<codec_id_string(enc->codec_id)
                            <<") type ("<<codec_type_string(enc->codec_type)
                            <<") already open, leaving it alone.");
                }
                assert(enc->codec_id);
                if (enc->channels > 2)
                    enc->channels = 2;
                bitrate += enc->bit_rate;
                break;
            }
            case CODEC_TYPE_SUBTITLE:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, QString("AVFD: subtitle codec (%1)")
                        .arg(codec_type_string(enc->codec_type)));
                break;
            }
            case CODEC_TYPE_DATA:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, QString("AVFD: data codec, ignoring (%1)")
                        .arg(codec_type_string(enc->codec_type)));
                break;
            }
            default:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, QString("AVFD: Unknown codec type (%1)")
                        .arg(codec_type_string(enc->codec_type)));
                break;
            }
        }

        if (enc->codec_type != CODEC_TYPE_AUDIO && 
            enc->codec_type != CODEC_TYPE_VIDEO &&
            enc->codec_type != CODEC_TYPE_SUBTITLE)
            continue;

        VERBOSE(VB_PLAYBACK, QString("AVFD: Looking for decoder for %1")
                .arg(codec_id_string(enc->codec_id)));
        AVCodec *codec = avcodec_find_decoder(enc->codec_id);
        if (!codec)
        {
            VERBOSE(VB_IMPORTANT, 
                    QString("AVFD: Could not find decoder for "
                            "codec (%1), ignoring.")
                    .arg(codec_id_string(enc->codec_id)));
            continue;
        }

        if (!enc->codec)
        {
            int open_val = avcodec_open(enc, codec);
            if (open_val < 0)
            {
                VERBOSE(VB_IMPORTANT, "AVFD: Could not "
                        "open codec "<<enc<<", "
                        <<"id("<<codec_id_string(enc->codec_id)<<") "
                        <<"type("<<codec_type_string(enc->codec_type)<<") "
                        <<"aborting. reason "<<open_val);
                //av_close_input_file(ic); // causes segfault
                ic = NULL;
                scanerror = -1;
                break;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "AVFD: Opened codec "<<enc<<", "
                        <<"id("<<codec_id_string(enc->codec_id)<<") "
                        <<"type("<<codec_type_string(enc->codec_type)<<")");
            }
        }

        if (enc->codec_type == CODEC_TYPE_SUBTITLE)
        {
            subtitleStreams.push_back( i );
        }

        if (enc->codec_type == CODEC_TYPE_AUDIO)
        {
            audioStreams.push_back( i );
            VERBOSE(VB_AUDIO, QString("AVFD: Stream #%1 (audio track #%2) is an "
                                      "audio stream with %3 channels.")
                    .arg(i).arg(audioStreams.size() - 1).arg(enc->channels));
        }
    }

    if (bitrate > 0)
        bitrate /= 1000;

    // Select a new track at the next opportunity.
    currentAudioTrack = -1;

    if (GetNVP()->IsErrored())
        scanerror = -1;

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
        AVCodecContext *enc = ic->streams[i]->codec;
        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                AVCodec *codec = enc->codec;
                if (!codec) {
                    VERBOSE(VB_IMPORTANT, QString("codec for stream %1 is null").arg(i));
                    break;
                }
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

    AVCodecContext *enc = ic->streams[wantedAudioStream]->codec;
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

    GetNVP()->SetAudioParams(16, audio_channels, audio_sampling_rate);
    GetNVP()->ReinitAudio();
    return;
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    VideoFrame *frame = nd->GetNVP()->GetNextVideoFrame(true);

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
    VideoFrame *frame = nd->GetNVP()->GetNextVideoFrame(false);

    pic->data[0] = frame->priv[0];
    pic->data[1] = frame->priv[1];
    pic->data[2] = frame->buf;

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

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

    if (s && src && s->opaque && src->opaque)
    {
        AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);

        int width = s->width;

        VideoFrame *frame = (VideoFrame *)src->opaque;
        nd->GetNVP()->DrawSlice(frame, 0, y, width, height);
    }
    else
        cerr<<"render_slice_xvmc called with bad avctx or src"<<endl;
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
                if (keyframedist > maxkeyframedist)
                    maxkeyframedist = keyframedist;

                if ((keyframedist == 15) ||
                    (keyframedist == 12))
                    positionMapType = MARK_GOP_START;
                else
                    positionMapType = MARK_GOP_BYFRAME;

                VERBOSE(VB_PLAYBACK, QString("Stream initial keyframedist: %1.").arg(keyframedist));
                GetNVP()->SetKeyframeDistance(keyframedist);
            }
        }
        else
        {
            if (keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
            {
                VERBOSE(VB_PLAYBACK, QString ("KeyFrameDistance has changed to %1 from %2.")
                                             .arg(tempKeyFrameDist).arg(keyframedist));

                keyframedist = tempKeyFrameDist;
                if (keyframedist > maxkeyframedist)
                    maxkeyframedist = keyframedist;

                if ((keyframedist == 15) ||
                    (keyframedist == 12))
                    positionMapType = MARK_GOP_START;
                else
                    positionMapType = MARK_GOP_BYFRAME;

                GetNVP()->SetKeyframeDistance(keyframedist);

#if 0
                // also reset length
                long long index = m_positionMap[m_positionMap.size() - 1].index;
                long long totframes = index * keyframedist;
                int length = (int)((totframes * 1.0) / fps);

                VERBOSE(VB_PLAYBACK, QString("Setting(2) length to: %1, "
                                             "total frames %2")
                                             .arg(length).arg((int)totframes));
                GetNVP()->SetFileLength(length, totframes);
#endif
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
            long long startpos = pkt->pos;
            VERBOSE(VB_PLAYBACK, QString("positionMap[ %1 ] == %2.").arg(prevgoppos / keyframedist)
                                         .arg((int)startpos));

            // Grow positionMap vector several entries at a time
            if (m_positionMap.capacity() == m_positionMap.size())
                m_positionMap.reserve(m_positionMap.size() + 60);
            PosMapEntry entry = {prevgoppos / keyframedist,
                                 prevgoppos, startpos};
            m_positionMap.push_back(entry);
        }

#if 0
        // If we are > 150 frames in and saw no positionmap at all, reset
        // length based on the actual bitrate seen so far
        if (framesRead > 150 && !recordingHasPositionMap && !livetv)
        {
            bitrate = (int)((pkt->pos * 8 * fps) / (framesRead - 1));
            float bytespersec = (float)bitrate / 8;
            float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
            GetNVP()->SetFileLength((int)(secs), (int)(secs * fps));
        }
#endif
    }
}

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void AvFormatDecoder::MpegPreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = stream->codec;
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
                    unsigned char *test = bufptr;
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

                        fps = 1 / av_q2d(context->time_base); 

                        align_dimensions(context, align_width, align_height);

                        GetNVP()->SetVideoParams(align_width,
                                                 align_height, fps,
                                                 keyframedist, aspect, 
                                                 kScan_Detect, true);
                        current_width = width;
                        current_height = height;
                        current_aspect = aspect;

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

        AVCodecContext *e = ic->streams[wantedAudioStream]->codec;
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

        AVCodecContext *e = ic->streams[wantedAudioStream]->codec;
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

    AVCodecContext *e = ic->streams[wantedAudioStream]->codec;
    SetupAudioStream();
    CheckAudioParams(e->sample_rate, e->channels, true);
    return true;
}

QStringList AvFormatDecoder::listAudioTracks() const
{
    QStringList list;
    int num_tracks = audioStreams.size();
    int track;
    
    for (track = 0; track < num_tracks; track++)
    {
        AVStream *s = ic->streams[audioStreams[track]];
        
        if (!s)
            continue;
        
        QString t = QString("%1: ").arg(track + 1);
        
        if (strlen(s->language) > 0)
        {
            t += iso639_toName((unsigned char *)(s->language));
            t += " ";
        }
        
        if (s->codec->codec_id == CODEC_ID_MP3 && s->codec->sub_id == 1)
            t += "MP1 ";
        else if (s->codec->codec_id == CODEC_ID_MP3 && s->codec->sub_id == 2)
            t += "MP2 ";
        else
        {      
            t += QString(s->codec->codec->name).upper();
            t += " ";
        }
        
        t += QString("%1ch").arg(s->codec->channels);
        
        list += t;
    }
    
    return list;
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
    {
        GetNVP()->SetAudioParams(-1, -1, -1);
        return false;
    }

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
            AVCodecContext *e = ic->streams[tempStream]->codec;

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
        if (selectedTrack == -1)
        {
            minChannels--;
        }
    }

    if (selectedTrack == -1)
    {
        VERBOSE(VB_AUDIO,
                QString("No suitable audio track exists."));
        return false;
    }

    currentAudioTrack = selectedTrack;
    wantedAudioStream = audioStreams[currentAudioTrack];
     
    AVCodecContext *e = ic->streams[wantedAudioStream]->codec;
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

    GetNVP()->SetEffDsp(curstream->codec->sample_rate * 100);

    do_ac3_passthru = curstream->codec->codec_id == CODEC_ID_AC3 &&
                      !transcoding &&
                      gContext->GetNumSetting("AC3PassThru", false);
}


void AvFormatDecoder::incCurrentSubtitleTrack()
{
    if (subtitleStreams.size())
    {
        int tempTrackNo = currentSubtitleTrack;
        ++tempTrackNo;
        if (tempTrackNo > (int)(subtitleStreams.size() - 1))
            tempTrackNo = 0;

        currentSubtitleTrack = tempTrackNo;
        wantedSubtitleStream = subtitleStreams[currentSubtitleTrack];
    }
}

void AvFormatDecoder::decCurrentSubtitleTrack()
{
    if (subtitleStreams.size())
    {
        int tempTrackNo = currentSubtitleTrack;
        --tempTrackNo;
        if (tempTrackNo < 0)
            tempTrackNo = subtitleStreams.size() - 1;

        currentSubtitleTrack = tempTrackNo;
        wantedSubtitleStream = subtitleStreams[currentSubtitleTrack];
    }
}

bool AvFormatDecoder::setCurrentSubtitleTrack(int trackNo)
{
    if (trackNo < 0)
        trackNo = -1;
    else if (trackNo >= (int)subtitleStreams.size())
        return false;

    currentSubtitleTrack = trackNo;
    if (currentSubtitleTrack < 0)
        return false;

    wantedSubtitleStream = subtitleStreams[currentSubtitleTrack];

    return true;
}

QStringList AvFormatDecoder::listSubtitleTracks() const
{
    QStringList list;
    int num_tracks = subtitleStreams.size();
    int track;
    
    for (track = 0; track < num_tracks; track++)
    {
        AVStream *s = ic->streams[subtitleStreams[track]];
        
        if (!s)
            continue;
        
        QString t = QString("%1: ").arg(track + 1);
        
        if (strlen(s->language) > 0)
        {
            t += iso639_toName((unsigned char *)(s->language));
            t += " ";
        }
         
        list += t;
    }
    
    return list;
}

// in case there's only one subtitle language available, always choose it
//
// if more than one subtitle languages are found, the best one is
// picked according to the ISO639Language[0..] settings
//
// in case there are no ISOLanguage[0..] settings, or no preferred language
// is found, the first found subtitle stream is chosen
bool AvFormatDecoder::autoSelectSubtitleTrack()
{
    if (!subtitleStreams.size())
    {
        currentSubtitleTrack = -1;
        wantedSubtitleStream = -1;
        return false;
    }

    int maxTracks = (subtitleStreams.size() - 1);

    int selectedTrack = -1;
    
    // go through all preferred languages and pick the best found
    QStringList langPref = iso639_get_language_list();
    QStringList::iterator l = langPref.begin();
    for (; l != langPref.end() && selectedTrack == -1; ++l)
    {
        for (int track = 0; track < maxTracks; track++)
        {
            int tempStream = subtitleStreams[track];
            AVStream* st = ic->streams[tempStream];

            if (st->language == *l)
            {
                selectedTrack = track;
                break;
            }
        }
    }
    
    if (selectedTrack == -1)
    {
        selectedTrack = 0;
    }

    currentSubtitleTrack = selectedTrack;
    wantedSubtitleStream = subtitleStreams[currentSubtitleTrack];
     
    return true;
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

    if ((currentSubtitleTrack == -1 || 
        currentSubtitleTrack >= (int)subtitleStreams.size()) &&
        subtitleStreams.size() > 0)
    {
        autoSelectSubtitleTrack();
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
            {
                pkt = new AVPacket;
                bzero(pkt, sizeof(AVPacket));
                av_init_packet(pkt);
            }

            if (av_read_frame(ic, pkt) < 0)
            {
                ateof = true;
                GetNVP()->SetEof();
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

        AVStream *curstream = ic->streams[pkt->stream_index];

        if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
            pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

        if (storevideoframes &&
            curstream->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            av_dup_packet(pkt);
            storedPackets.append(pkt);
            pkt = NULL;
            continue;
        }

        if (len > 0 && curstream->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            if (framesRead == 0 && !(pkt->flags & PKT_FLAG_KEY))
            {
                av_free_packet(pkt);
                continue;
            }

            framesRead++;
            if (exitafterdecoded)
                gotvideo = 1;

            AVCodecContext *context = curstream->codec;

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

        if (!curstream->codec->codec)
        {
            VERBOSE(VB_PLAYBACK, QString("No codec for stream index %1").arg(pkt->stream_index));
            av_free_packet(pkt);
            continue;
        }

        firstloop = true;
        have_err = false;

        while (!have_err && len > 0)
        {
            pthread_mutex_lock(&avcodeclock);
            int ctype = curstream->codec->codec_type;
            pthread_mutex_unlock(&avcodeclock);
            switch (ctype)
            {
                case CODEC_TYPE_AUDIO:
                {
                    if (firstloop && pkt->pts != (int64_t)AV_NOPTS_VALUE)
                        lastapts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);

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

                    pthread_mutex_lock(&avcodeclock);
                    if (do_ac3_passthru)
                    {
                        data_size = pkt->size;
                        ret = EncodeAC3Frame(ptr, len, audioSamples,
                                             data_size);
                    }
                    else
                    {
                        ret = avcodec_decode_audio(curstream->codec,
                                                   audioSamples, &data_size,
                                                   ptr, len);
                    }
                    pthread_mutex_unlock(&avcodeclock);

                    ptr += ret;
                    len -= ret;

                    if (data_size <= 0)
                        continue;

                    if (!do_ac3_passthru)
                        CheckAudioParams(curstream->codec->sample_rate,
                                         curstream->codec->channels, false);

                    long long temppts = lastapts;

                    // calc for next frame
                    lastapts += (long long)((double)(data_size * 1000) /
                                (curstream->codec->channels * 2) / 
                                curstream->codec->sample_rate);

                    GetNVP()->AddAudioData((char *)audioSamples, data_size, temppts);

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

                    AVCodecContext *context = curstream->codec;
                    AVFrame mpa_pic;
                    bzero(&mpa_pic, sizeof(AVFrame));

                    int gotpicture = 0;

                    pthread_mutex_lock(&avcodeclock);
                    if (d->HasMPEG2Dec())
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
 
                        picframe = GetNVP()->GetNextVideoFrame();

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
                    GetNVP()->ReleaseNextVideoFrame(picframe, temppts);
                    if (!directrendering)
                        GetNVP()->DiscardVideoFrame(picframe);

                    gotvideo = 1;
                    framesPlayed++;

                    lastvpts = temppts;
                    break;
                }
                case CODEC_TYPE_SUBTITLE:
                {
                    AVCodecContext *context = curstream->codec;
                    int gotSubtitles = 0;
                    AVSubtitle subtitle;

                    if (pkt->stream_index == wantedSubtitleStream)
                    {
                        avcodec_decode_subtitle(context, &subtitle, 
                                                &gotSubtitles, ptr, len);
                    }

                    // the subtitle decoder always consumes the whole packet
                    ptr += len;
                    len = 0;

                    if (gotSubtitles) 
                    {
                        subtitle.start_display_time += pts;
                        subtitle.end_display_time += pts;
                        GetNVP()->AddSubtitle(subtitle);
                    }

                    break;
                }
                default:
                    cerr << "error decoding - " << curstream->codec->codec_type
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

