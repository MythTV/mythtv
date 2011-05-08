// C headers
#include <cassert>
#include <unistd.h>
#include <cmath>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

#include <QTextCodec>

// MythTV headers
#include "mythconfig.h"
#include "avformatdecoder.h"
#include "privatedecoder.h"
#include "audiooutput.h"
#include "audiooutpututil.h"
#include "RingBuffer.h"
#include "mythplayer.h"
#include "remoteencoder.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "mythdbcon.h"
#include "iso639.h"
#include "mpegtables.h"
#include "atscdescriptors.h"
#include "dvbdescriptors.h"
#include "cc608decoder.h"
#include "cc708decoder.h"
#include "teletextdecoder.h"
#include "subtitlereader.h"
#include "interactivetv.h"
#include "DVDRingBuffer.h"
#include "BDRingBuffer.h"
#include "videodisplayprofile.h"
#include "mythuihelper.h"

#include "lcddevice.h"

#include "videoout_quartz.h"  // For VOQ::GetBestSupportedCodec()

#ifdef USING_XVMC
#include "videoout_xv.h"
extern "C" {
#include "libavcodec/xvmc.h"
}
#endif // USING_XVMC

#ifdef USING_VDPAU
#include "videoout_vdpau.h"
extern "C" {
#include "libavcodec/vdpau.h"
}
#endif // USING_VDPAU

extern "C" {
#include "libavutil/avutil.h"
#include "libavcodec/ac3_parser.h"
extern const uint8_t *ff_find_start_code(const uint8_t *p, const uint8_t *end, uint32_t *state);
extern void ff_read_frame_flush(AVFormatContext *s);
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#if CONFIG_LIBMPEG2EXTERNAL
#include <mpeg2dec/mpeg2.h>
#else
#include "../libmythmpeg2/mpeg2.h"
#endif
#include "ivtv_myth.h"
}

#define LOC QString("AFD: ")
#define LOC_ERR QString("AFD Error: ")
#define LOC_WARN QString("AFD Warning: ")

#define MAX_AC3_FRAME_SIZE 6144

static const float eps = 1E-5;

static const int max_video_queue_size = 180;

static int cc608_parity(uint8_t byte);
static int cc608_good_parity(const int *parity_table, uint16_t data);
static void cc608_build_parity_table(int *parity_table);

static int dts_syncinfo(uint8_t *indata_ptr, int *flags,
                        int *sample_rate, int *bit_rate);
static int dts_decode_header(uint8_t *indata_ptr, int *rate,
                             int *nblks, int *sfreq);
static int encode_frame(bool dts, unsigned char* data, int len,
                        short *samples, int &samples_size);
static QSize get_video_dim(const AVCodecContext &ctx)
{
    return QSize(ctx.width >> ctx.lowres, ctx.height >> ctx.lowres);
}
static float get_aspect(const AVCodecContext &ctx)
{
    float aspect_ratio = 0.0f;

    if (ctx.sample_aspect_ratio.num && ctx.height)
    {
        aspect_ratio = av_q2d(ctx.sample_aspect_ratio) * (float) ctx.width;
        aspect_ratio /= (float) ctx.height;
    }

    if (aspect_ratio <= 0.0f || aspect_ratio > 6.0f)
    {
        if (ctx.height)
            aspect_ratio = (float)ctx.width / (float)ctx.height;
        else
            aspect_ratio = 4.0f / 3.0f;
    }

    return aspect_ratio;
}

int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic);
void render_slice_xvmc(struct AVCodecContext *s, const AVFrame *src,
                       int offset[4], int y, int type, int height);

int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void render_slice_vdpau(struct AVCodecContext *s, const AVFrame *src,
                        int offset[4], int y, int type, int height);

static AVCodec *find_vdpau_decoder(AVCodec *c, enum CodecID id)
{
    AVCodec *codec = c;
    while (codec)
    {
        if (codec->id == id && CODEC_IS_VDPAU(codec))
            return codec;

        codec = codec->next;
    }

    return c;
}

static void myth_av_log(void *ptr, int level, const char* fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE)
        return;

    static QString full_line("");
    static const int msg_len = 255;
    static QMutex string_lock;
    uint verbose_level = 0;

    // determine mythtv debug level from av log level
    switch (level)
    {
        case AV_LOG_PANIC:
        case AV_LOG_FATAL:
            verbose_level = VB_IMPORTANT;
            break;
        case AV_LOG_ERROR:
            verbose_level = VB_GENERAL;
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
        case AV_LOG_INFO:
            verbose_level = VB_EXTRA;
        case AV_LOG_WARNING:
            verbose_level |= VB_LIBAV;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_level))
        return;

    string_lock.lock();
    if (full_line.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        full_line.sprintf("[%s @ %p]", avc->item_name(ptr), avc);
    }

    char str[msg_len+1];
    int bytes = vsnprintf(str, msg_len+1, fmt, vl);
    // check for truncated messages and fix them
    if (bytes > msg_len)
    {
        VERBOSE(VB_IMPORTANT, QString("Libav log output truncated %1 of %2 bytes written")
                .arg(msg_len).arg(bytes));
        str[msg_len-1] = '\n';
    }

    full_line += QString(str);
    if (full_line.endsWith("\n"))
    {
        full_line.truncate(full_line.length() - 1);
        VERBOSE(verbose_level, full_line);
        full_line.truncate(0);
    }
    string_lock.unlock();
}

static int get_canonical_lang(const char *lang_cstr)
{
    if (lang_cstr[0] == '\0' || lang_cstr[1] == '\0')
    {
        return iso639_str3_to_key("und");
    }
    else if (lang_cstr[2] == '\0')
    {
        QString tmp2 = lang_cstr;
        QString tmp3 = iso639_str2_to_str3(tmp2);
        int lang = iso639_str3_to_key(tmp3);
        return iso639_key_to_canonical_key(lang);
    }
    else
    {
        int lang = iso639_str3_to_key(lang_cstr);
        return iso639_key_to_canonical_key(lang);
    }
}

void AvFormatDecoder::GetDecoders(render_opts &opts)
{
    opts.decoders->append("ffmpeg");
    (*opts.equiv_decoders)["ffmpeg"].append("nuppel");
    (*opts.equiv_decoders)["ffmpeg"].append("dummy");

#ifdef USING_XVMC
    opts.decoders->append("xvmc");
    opts.decoders->append("xvmc-vld");
    (*opts.equiv_decoders)["xvmc"].append("dummy");
    (*opts.equiv_decoders)["xvmc-vld"].append("dummy");
#endif

#ifdef USING_VDPAU
    opts.decoders->append("vdpau");
    (*opts.equiv_decoders)["vdpau"].append("dummy");
#endif

    PrivateDecoder::GetDecoders(opts);
}

AvFormatDecoder::AvFormatDecoder(MythPlayer *parent,
                                 const ProgramInfo &pginfo,
                                 bool use_null_videoout,
                                 bool allow_private_decode,
                                 bool no_hardware_decode,
                                 AVSpecialDecode special_decoding)
    : DecoderBase(parent, pginfo),
      private_dec(NULL),
      is_db_ignored(gCoreContext->IsDatabaseIgnored()),
      m_h264_parser(new H264Parser()),
      ic(NULL),
      frame_decoded(0),             decoded_video_frame(NULL),
      avfRingBuffer(NULL),          sws_ctx(NULL),
      directrendering(false),
      gopset(false),                seen_gop(false),
      seq_count(0),
      prevgoppos(0),                gotvideo(false),
      skipaudio(false),             allowedquit(false),
      start_code_state(0xffffffff),
      lastvpts(0),                  lastapts(0),
      lastccptsu(0),
      faulty_pts(0),                faulty_dts(0),
      last_pts_for_fault_detection(0),
      last_dts_for_fault_detection(0),
      pts_detected(false),
      reordered_pts_detected(false),
      pts_selected(true),
      using_null_videoout(use_null_videoout),
      video_codec_id(kCodec_NONE),
      no_hardware_decoders(no_hardware_decode),
      allow_private_decoders(allow_private_decode),
      special_decode(special_decoding),
      maxkeyframedist(-1),
      // Closed Caption & Teletext decoders
      ccd608(new CC608Decoder(parent->GetCC608Reader())),
      ccd708(new CC708Decoder(parent->GetCC708Reader())),
      ttd(new TeletextDecoder(parent->GetTeletextReader())),
      subReader(parent->GetSubReader()),
      // Interactive TV
      itv(NULL),
      // Audio
      audioSamples(NULL),
      internal_vol(false),
      disable_passthru(false),
      dummy_frame(NULL),
      // DVD
      dvd_xvmc_enabled(false), dvd_video_codec_changed(false),
      m_fps(0.0f)
{
    bzero(&params, sizeof(AVFormatParameters));
    bzero(&readcontext, sizeof(readcontext));
    // using preallocated AVFormatContext for our own ByteIOContext
    params.prealloced_context = 1;
    audioSamples = (short int *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                           sizeof(int32_t));
    ccd608->SetIgnoreTimecode(true);

    bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(myth_av_log);

    internal_vol = gCoreContext->GetNumSetting("MythControlsVolume", 0);

    audioIn.sample_size = -32; // force SetupAudioStream to run once
    itv = m_parent->GetInteractiveTV();

    cc608_build_parity_table(cc608_parity_table);

    if (gCoreContext->GetNumSetting("CCBackground", 0))
        CC708Window::forceWhiteOnBlackText = true;

    no_dts_hack = false;

    int x = gCoreContext->GetNumSetting("CommFlagFast", 0);
    VERBOSE(VB_COMMFLAG, LOC + QString("CommFlagFast: %1").arg(x));
    if (x == 0)
        special_decode = kAVSpecialDecode_None;
    VERBOSE(VB_COMMFLAG, LOC + QString("Special Decode Flags: 0x%1")
        .arg(special_decode, 0, 16));
}

AvFormatDecoder::~AvFormatDecoder()
{
    while (!storedPackets.isEmpty())
    {
        AVPacket *pkt = storedPackets.takeFirst();
        av_free_packet(pkt);
        delete pkt;
    }

    CloseContext();
    delete ccd608;
    delete ccd708;
    delete ttd;
    delete private_dec;
    delete m_h264_parser;

    sws_freeContext(sws_ctx);

    av_freep((void *)&audioSamples);

    if (dummy_frame)
    {
        delete [] dummy_frame->buf;
        delete dummy_frame;
        dummy_frame = NULL;
    }

    if (avfRingBuffer)
        delete avfRingBuffer;

    if (LCD *lcd = LCD::Get())
    {
        lcd->setAudioFormatLEDs(AUDIO_AC3, false);
        lcd->setVideoFormatLEDs(VIDEO_MPG, false);
        lcd->setVariousLEDs(VARIOUS_HDTV, false);
        lcd->setVariousLEDs(VARIOUS_SPDIF, false);
        lcd->setSpeakerLEDs(SPEAKER_71, false);    // should clear any and all speaker LEDs
    }
}

void AvFormatDecoder::CloseCodecs()
{
    if (ic)
    {
        for (uint i = 0; i < ic->nb_streams; i++)
        {
            QMutexLocker locker(avcodeclock);
            AVStream *st = ic->streams[i];
            if (st->codec->codec)
                avcodec_close(st->codec);
        }
    }
}

void AvFormatDecoder::CloseContext()
{
    if (ic)
    {
        CloseCodecs();

        AVInputFormat *fmt = ic->iformat;
        ic->iformat->flags |= AVFMT_NOFILE;

        av_free(ic->pb->buffer);
        av_free(ic->pb);
        av_close_input_file(ic);
        ic = NULL;
        fmt->flags &= ~AVFMT_NOFILE;
    }

    delete private_dec;
    private_dec = NULL;
    m_h264_parser->Reset();
}

static int64_t lsb3full(int64_t lsb, int64_t base_ts, int lsb_bits)
{
    int64_t mask = (lsb_bits < 64) ? (1LL<<lsb_bits)-1 : -1LL;
    return  ((lsb - base_ts)&mask);
}

int64_t AvFormatDecoder::NormalizeVideoTimecode(int64_t timecode)
{
    int64_t start_pts = 0, pts;

    AVStream *st = NULL;
    for (uint i = 0; i < ic->nb_streams; i++)
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

    if (ic->start_time != AV_NOPTS_VALUE)
        start_pts = av_rescale(ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);

    pts = av_rescale(timecode / 1000.0,
                     st->time_base.den,
                     st->time_base.num);

    // adjust for start time and wrap
    pts = lsb3full(pts, start_pts, st->pts_wrap_bits);

    return (int64_t)(av_q2d(st->time_base) * pts * 1000);
}

int64_t AvFormatDecoder::NormalizeVideoTimecode(AVStream *st,
                                                int64_t timecode)
{
    int64_t start_pts = 0, pts;

    if (ic->start_time != AV_NOPTS_VALUE)
        start_pts = av_rescale(ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);

    pts = av_rescale(timecode / 1000.0,
                     st->time_base.den,
                     st->time_base.num);

    // adjust for start time and wrap
    pts = lsb3full(pts, start_pts, st->pts_wrap_bits);

    return (int64_t)(av_q2d(st->time_base) * pts * 1000);
}

int AvFormatDecoder::GetNumChapters()
{
    if (ic->nb_chapters > 1)
        return ic->nb_chapters;
    return 0;
}

void AvFormatDecoder::GetChapterTimes(QList<long long> &times)
{
    int total = GetNumChapters();
    if (!total)
        return;

    for (int i = 0; i < total; i++)
    {
        int num = ic->chapters[i]->time_base.num;
        int den = ic->chapters[i]->time_base.den;
        int64_t start = ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num / (long double)den;
        times.push_back((long long)total_secs);
    }
}

int AvFormatDecoder::GetCurrentChapter(long long framesPlayed)
{
    if (!GetNumChapters())
        return 0;

    for (int i = (ic->nb_chapters - 1); i > -1 ; i--)
    {
        int num = ic->chapters[i]->time_base.num;
        int den = ic->chapters[i]->time_base.den;
        int64_t start = ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num / (long double)den;
        long long framenum = (long long)(total_secs * fps);
        if (framesPlayed >= framenum)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("GetCurrentChapter(selected chapter %1 framenum %2)")
                            .arg(i + 1).arg(framenum));
            return i + 1;
        }
    }
    return 0;
}

long long AvFormatDecoder::GetChapter(int chapter)
{
    if (chapter < 1 || chapter > GetNumChapters())
        return -1;

    int num = ic->chapters[chapter - 1]->time_base.num;
    int den = ic->chapters[chapter - 1]->time_base.den;
    int64_t start = ic->chapters[chapter - 1]->start;
    long double total_secs = (long double)start * (long double)num / (long double)den;
    long long framenum = (long long)(total_secs * fps);
    VERBOSE(VB_PLAYBACK, LOC + QString("GetChapter %1: framenum %2")
                               .arg(chapter).arg(framenum));
    return framenum;
}

bool AvFormatDecoder::DoRewind(long long desiredFrame, bool discardFrames)
{
    VERBOSE(VB_PLAYBACK, LOC + "DoRewind("
            <<desiredFrame<<", "
            <<( discardFrames ? "do" : "don't" )<<" discard frames)");

    if (recordingHasPositionMap || livetv)
        return DecoderBase::DoRewind(desiredFrame, discardFrames);

    dorewind = true;

    // avformat-based seeking
    return DoFastForward(desiredFrame, discardFrames);
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame, bool discardFrames)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("DoFastForward(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (recordingHasPositionMap || livetv)
        return DecoderBase::DoFastForward(desiredFrame, discardFrames);

    bool oldrawstate = getrawframes;
    getrawframes = false;

    AVStream *st = NULL;
    for (uint i = 0; i < ic->nb_streams; i++)
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

    int seekDelta = desiredFrame - framesPlayed;

    // avoid using av_frame_seek if we are seeking frame-by-frame when paused
    if (seekDelta >= 0 && seekDelta < 2 && !dorewind && m_parent->GetPlaySpeed() == 0.0f)
    {
        SeekReset(framesPlayed, seekDelta, false, true);
        m_parent->SetFramesPlayed(framesPlayed + 1);
        m_parent->getVideoOutput()->SetFramesPlayed(framesPlayed + 1);

        return true;
    }

    AVCodecContext *context = st->codec;

    long long ts = 0;
    if (ic->start_time != (int64_t)AV_NOPTS_VALUE)
        ts = ic->start_time;

    // convert framenumber to normalized timestamp
    long double seekts = desiredFrame * AV_TIME_BASE / fps;
    ts += (long long)seekts;

    bool exactseeks = DecoderBase::getExactSeeks();

    int flags = (dorewind || exactseeks) ? AVSEEK_FLAG_BACKWARD : 0;

    if (av_seek_frame(ic, -1, ts, flags) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR
                <<"av_seek_frame(ic, -1, "<<ts<<", 0) -- error");
        return false;
    }

    int normalframes = 0;

    if (st->cur_dts != (int64_t)AV_NOPTS_VALUE)
    {

        int64_t adj_cur_dts = st->cur_dts;

        if (ic->start_time != (int64_t)AV_NOPTS_VALUE)
        {
            int64_t st1 = av_rescale(ic->start_time,
                                    st->time_base.den,
                                    AV_TIME_BASE * (int64_t)st->time_base.num);
            adj_cur_dts = lsb3full(adj_cur_dts, st1, st->pts_wrap_bits);
        }

        int64_t adj_seek_dts = av_rescale(seekts,
                                          st->time_base.den,
                                          AV_TIME_BASE * (int64_t)st->time_base.num);

        int64_t max_dts = (st->pts_wrap_bits < 64) ? (1LL<<st->pts_wrap_bits)-1 : -1LL;

        // When seeking near the start of a stream the current dts is sometimes
        // less than the start time which causes lsb3full to return adj_cur_dts
        // close to the maximum dts value. If so, set adj_cur_dts to zero.
        if (adj_seek_dts < max_dts / 64 && adj_cur_dts > max_dts / 2)
            adj_cur_dts = 0;

        long long newts = av_rescale(adj_cur_dts,
                                (int64_t)AV_TIME_BASE * (int64_t)st->time_base.num,
                                st->time_base.den);

        lastKey = (long long)((newts*(long double)fps)/AV_TIME_BASE);
        framesPlayed = lastKey;
        framesRead = lastKey;

        normalframes = (exactseeks) ? desiredFrame - framesPlayed : 0;
        normalframes = max(normalframes, 0);
        no_dts_hack = false;
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + "No DTS Seeking Hack!");
        no_dts_hack = true;
        framesPlayed = desiredFrame;
        framesRead = desiredFrame;
        normalframes = 0;
    }

    SeekReset(lastKey, normalframes, true, discardFrames);

    if (discardFrames)
    {
        m_parent->SetFramesPlayed(framesPlayed + 1);
        m_parent->getVideoOutput()->SetFramesPlayed(framesPlayed + 1);
    }

    dorewind = false;

    getrawframes = oldrawstate;

    return true;
}

void AvFormatDecoder::SeekReset(long long newKey, uint skipFrames,
                                bool doflush, bool discardFrames)
{
    if (ringBuffer->isDVD())
    {
        if (ringBuffer->InDVDMenuOrStillFrame() ||
            newKey == 0)
            return;
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("SeekReset(%1, %2, %3 flush, %4 discard)")
            .arg(newKey).arg(skipFrames)
            .arg((doflush) ? "do" : "don't")
            .arg((discardFrames) ? "do" : "don't"));

    DecoderBase::SeekReset(newKey, skipFrames, doflush, discardFrames);

    if (doflush)
    {
        lastapts = 0;
        lastvpts = 0;
        lastccptsu = 0;
        faulty_pts = faulty_dts = 0;
        last_pts_for_fault_detection = 0;
        last_dts_for_fault_detection = 0;
        pts_detected = false;
        reordered_pts_detected = false;

        ff_read_frame_flush(ic);

        // Only reset the internal state if we're using our seeking,
        // not when using libavformat's seeking
        if (recordingHasPositionMap || livetv)
        {
            ic->pb->pos = ringBuffer->GetReadPosition();
            ic->pb->buf_ptr = ic->pb->buffer;
            ic->pb->buf_end = ic->pb->buffer;
            ic->pb->eof_reached = 0;
        }

        // Flush the avcodec buffers
        VERBOSE(VB_PLAYBACK, LOC + "SeekReset() flushing");
        for (uint i = 0; i < ic->nb_streams; i++)
        {
            AVCodecContext *enc = ic->streams[i]->codec;
            if (enc->codec)
                avcodec_flush_buffers(enc);
        }
        if (private_dec)
            private_dec->Reset();
    }

    // Discard all the queued up decoded frames
    if (discardFrames)
        m_parent->DiscardVideoFrames(doflush);

    if (doflush)
    {
        // Free up the stored up packets
        while (!storedPackets.isEmpty())
        {
            AVPacket *pkt = storedPackets.takeFirst();
            av_free_packet(pkt);
            delete pkt;
        }

        prevgoppos = 0;
        gopset = false;
        if (!ringBuffer->isDVD())
        {
            if (!no_dts_hack)
            {
                framesPlayed = lastKey;
                framesRead = lastKey;
            }

            no_dts_hack = false;
        }
    }

    // Skip all the desired number of skipFrames
    for (;skipFrames > 0 && !ateof; skipFrames--)
    {
        GetFrame(kDecodeVideo);
        if (decoded_video_frame)
            m_parent->DiscardVideoFrame(decoded_video_frame);
    }
}

void AvFormatDecoder::SetEof(bool eof)
{
    if (!eof && ic && ic->pb)
    {
        VERBOSE(VB_IMPORTANT, LOC +
            QString("Resetting byte context eof (livetv %1 was eof %2)")
            .arg(livetv).arg(ic->pb->eof_reached));
        ic->pb->eof_reached = 0;
    }
    DecoderBase::SetEof(eof);
}

void AvFormatDecoder::Reset(bool reset_video_data, bool seek_reset)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("Reset(%1, %2)")
            .arg(reset_video_data).arg(seek_reset));
    if (seek_reset)
        SeekReset(0, 0, true, false);

    if (reset_video_data)
    {
        ResetPosMap();
        framesPlayed = 0;
        framesRead = 0;
        seen_gop = false;
        seq_count = 0;
    }
}

void AvFormatDecoder::Reset()
{
    DecoderBase::Reset();

    if (ringBuffer->isDVD())
        SyncPositionMap();
}

bool AvFormatDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize],
                                const QString &filename, int testbufsize)
{
    {
        QMutexLocker locker(avcodeclock);
        av_register_all();
    }

    AVProbeData probe;

    QByteArray fname = filename.toAscii();
    probe.filename = fname.constData();
    probe.buf = (unsigned char *)testbuf;
    probe.buf_size = testbufsize;

    int score = AVPROBE_SCORE_MAX/4;

    if (testbufsize + AVPROBE_PADDING_SIZE > kDecoderProbeBufferSize)
    {
        probe.buf_size = kDecoderProbeBufferSize - AVPROBE_PADDING_SIZE;
        score = 0;
    }
    else if (testbufsize*2 >= kDecoderProbeBufferSize)
    {
        score--;
    }

    if (av_probe_input_format2(&probe, true, &score))
        return true;
    return false;
}

void AvFormatDecoder::InitByteContext(void)
{
    int streamed = 0;
    int buffer_size = 32768;

    if (ringBuffer->isDVD()) {
        streamed = 1;
        buffer_size = 2048;
    }
    else if (ringBuffer->LiveMode())
        streamed = 1;

    readcontext.prot = &AVF_RingBuffer_Protocol;
    readcontext.flags = 0;
    readcontext.is_streamed = streamed;
    readcontext.max_packet_size = 0;
    readcontext.priv_data = avfRingBuffer;

    unsigned char* buffer = (unsigned char *)av_malloc(buffer_size);

    ic->pb = av_alloc_put_byte(buffer, buffer_size, 0,
                               &readcontext,
                               AVF_Read_Packet,
                               AVF_Write_Packet,
                               AVF_Seek_Packet);

    ic->pb->is_streamed = streamed;
}

extern "C" void HandleStreamChange(void* data)
{
    AvFormatDecoder* decoder = (AvFormatDecoder*) data;
    int cnt = decoder->ic->nb_streams;

    VERBOSE(VB_PLAYBACK, LOC + "HandleStreamChange(): "
            "streams_changed "<<data<<" -- stream count "<<cnt);

    QMutexLocker locker(avcodeclock);
    decoder->SeekReset(0, 0, true, true);
    decoder->ScanStreams(false);
}

extern "C" void HandleDVDStreamChange(void* data)
{
    AvFormatDecoder* decoder = (AvFormatDecoder*) data;
    int cnt = decoder->ic->nb_streams;

    VERBOSE(VB_PLAYBACK, LOC + "HandleDVDStreamChange(): "
            "streams_changed "<<data<<" -- stream count "<<cnt);

    QMutexLocker locker(avcodeclock);
    //decoder->SeekReset(0, 0, true, true);
    decoder->ScanStreams(true);
}

/**
 *  OpenFile opens a ringbuffer for playback.
 *
 *  OpenFile deletes any existing context then use testbuf to
 *  guess at the stream type. It then calls ScanStreams to find
 *  any valid streams to decode. If possible a position map is
 *  also built for quick skipping.
 *
 *  \param rbuffer pointer to a valid ringuffer.
 *  \param novideo if true then no video is sought in ScanSreams.
 *  \param testbuf this parameter is not used by AvFormatDecoder.
 */
int AvFormatDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                              char testbuf[kDecoderProbeBufferSize],
                              int testbufsize)
{
    CloseContext();

    ringBuffer = rbuffer;

    if (avfRingBuffer)
        delete avfRingBuffer;
    avfRingBuffer = new AVFRingBuffer(rbuffer);

    AVInputFormat *fmt      = NULL;
    QString        fnames   = ringBuffer->GetFilename();
    QByteArray     fnamea   = fnames.toAscii();
    const char    *filename = fnamea.constData();

    AVProbeData probe;
    probe.filename = filename;
    probe.buf = (unsigned char *)testbuf;
    if (testbufsize + AVPROBE_PADDING_SIZE <= kDecoderProbeBufferSize)
        probe.buf_size = testbufsize;
    else
        probe.buf_size = kDecoderProbeBufferSize - AVPROBE_PADDING_SIZE;

    fmt = av_probe_input_format(&probe, true);
    if (!fmt)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Probe failed for file: \"%1\".").arg(filename));
        return -1;
    }

    fmt->flags |= AVFMT_NOFILE;

    ic = avformat_alloc_context();
    if (!ic)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not allocate format context.");
        return -1;
    }

    InitByteContext();

    int err = av_open_input_stream(&ic, ic->pb, filename, fmt, &params);
    if (err < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR
                <<"avformat err("<<err<<") on av_open_input_file call.");
        return -1;
    }

    int ret = -1;
    {
        QMutexLocker locker(avcodeclock);
        ret = av_find_stream_info(ic);
    }
    if (ringBuffer->isDVD())
    {
        if (!ringBuffer->DVD()->StartFromBeginning())
            return -1;
        ringBuffer->DVD()->IgnoreStillOrWait(false);
    }
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not find codec parameters. " +
                QString("file was \"%1\".").arg(filename));
        av_close_input_file(ic);
        ic = NULL;
        return -1;
    }
    ic->streams_changed = HandleStreamChange;
    if (ringBuffer->isDVD())
        ic->streams_changed = HandleDVDStreamChange;
    ic->stream_change_data = this;

    fmt->flags &= ~AVFMT_NOFILE;

    if (!ringBuffer->isDVD() && !ringBuffer->isBD() && !livetv)
    {
        av_estimate_timings(ic, 0);
        // generate timings based on the video stream to avoid bogus ffmpeg
        // values for duration and bitrate
        av_update_stream_timings_video(ic);
    }

    // Scan for the initial A/V streams
    ret = ScanStreams(novideo);
    if (-1 == ret)
        return ret;

    AutoSelectTracks(); // This is needed for transcoder

#ifdef USING_MHEG
    {
        int initialAudio = -1, initialVideo = -1;
        if (itv || (itv = m_parent->GetInteractiveTV()))
            itv->GetInitialStreams(initialAudio, initialVideo);
        if (initialAudio >= 0)
            SetAudioByComponentTag(initialAudio);
        if (initialVideo >= 0)
            SetVideoByComponentTag(initialVideo);
    }
#endif // USING_MHEG

    // Try to get a position map from the recorder if we don't have one yet.
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

    // If watching pre-recorded television or video use ffmpeg duration
    int64_t dur = ic->duration / (int64_t)AV_TIME_BASE;
    if (dur > 0 && !livetv && !watchingrecording)
    {
        m_parent->SetDuration((int)dur);
    }

    // If we don't have a position map, set up ffmpeg for seeking
    if (!recordingHasPositionMap && !livetv)
    {
        VERBOSE(VB_PLAYBACK, LOC +
                "Recording has no position -- using libavformat seeking.");

        if (dur > 0)
        {
            m_parent->SetFileLength((int)(dur), (int)(dur * fps));
        }
        else
        {
            // the pvr-250 seems to over report the bitrate by * 2
            float bytespersec = (float)bitrate / 8 / 2;
            float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
            m_parent->SetFileLength((int)(secs), (int)(secs * fps));
        }

        // we will not see a position map from db or remote encoder,
        // set the gop interval to 15 frames.  if we guess wrong, the
        // auto detection will change it.
        keyframedist = 15;
        positionMapType = MARK_GOP_BYFRAME;

        if (!strcmp(fmt->name, "avi"))
        {
            // avi keyframes are too irregular
            keyframedist = 1;
        }

        dontSyncPositionMap = true;
        ic->build_index = 1;
    }
    // we have a position map, disable libavformat's seek index
    else
        ic->build_index = 0;

    dump_format(ic, 0, filename, 0);

    // print some useful information if playback debugging is on
    if (hasFullPositionMap)
        VERBOSE(VB_PLAYBACK, LOC + "Position map found");
    else if (recordingHasPositionMap)
        VERBOSE(VB_PLAYBACK, LOC + "Partial position map found");
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Successfully opened decoder for file: "
                    "\"%1\". novideo(%2)").arg(filename).arg(novideo));

    // Print AVChapter information
    for (unsigned int i=0; i < ic->nb_chapters; i++)
    {
        int num = ic->chapters[i]->time_base.num;
        int den = ic->chapters[i]->time_base.den;
        int64_t start = ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num / (long double)den;
        int hours = (int)total_secs / 60 / 60;
        int minutes = ((int)total_secs / 60) - (hours * 60);
        double secs = (double)total_secs - (double)(hours * 60 * 60 + minutes * 60);
        long long framenum = (long long)(total_secs * fps);
        VERBOSE(VB_PLAYBACK, LOC + QString("Chapter %1 found @ [%2:%3:%4]->%5")
                .arg(QString().sprintf("%02d", i + 1))
                .arg(QString().sprintf("%02d", hours))
                .arg(QString().sprintf("%02d", minutes))
                .arg(QString().sprintf("%06.3f", secs))
                .arg(framenum));
    }

    // Return true if recording has position map
    return recordingHasPositionMap;
}

float AvFormatDecoder::normalized_fps(AVStream *stream, AVCodecContext *enc)
{
    float fps, avg_fps, stream_fps, container_fps, estimated_fps;
    avg_fps = stream_fps = container_fps = estimated_fps = 0.0f;

    if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
        avg_fps = av_q2d(stream->avg_frame_rate); // MKV default_duration

    if (enc->time_base.den && enc->time_base.num) // tbc
        stream_fps = 1.0f / av_q2d(enc->time_base) / enc->ticks_per_frame;
    // Some formats report fps waaay too high. (wrong time_base)
    if (stream_fps > 121.0f && (enc->time_base.den > 10000) &&
        (enc->time_base.num == 1))
    {
        enc->time_base.num = 1001;  // seems pretty standard
        if (av_q2d(enc->time_base) > 0)
            stream_fps = 1.0f / av_q2d(enc->time_base);
    }
    if (stream->time_base.den && stream->time_base.num) // tbn
        container_fps = 1.0f / av_q2d(stream->time_base);
    if (stream->r_frame_rate.den && stream->r_frame_rate.num) // tbr
        estimated_fps = av_q2d(stream->r_frame_rate);

    if (QString(ic->iformat->name).contains("matroska") && 
        avg_fps < 121.0f && avg_fps > 3.0f)
        fps = avg_fps; // matroska default_duration
    else if (QString(ic->iformat->name).contains("avi") && 
        container_fps < 121.0f && container_fps > 3.0f)
        fps = container_fps; // avi uses container fps for timestamps
    else if (stream_fps < 121.0f && stream_fps > 3.0f) 
        fps = stream_fps;
    else if (container_fps < 121.0f && container_fps > 3.0f) 
        fps = container_fps;
    else if (estimated_fps < 70.0f && estimated_fps > 20.0f) 
        fps = estimated_fps;
    else
        fps = stream_fps;

    // If it is still out of range, just assume NTSC...
    fps = (fps > 121.0f) ? (30000.0f / 1001.0f) : fps;
    if (fps != m_fps)
    {
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Selected FPS is %1 (avg %2 stream %3 "
                        "container %4 estimated %5)").arg(fps).arg(avg_fps)
                        .arg(stream_fps).arg(container_fps).arg(estimated_fps));
        m_fps = fps;
    }

    return fps;
}

static bool IS_XVMC_VLD_PIX_FMT(enum PixelFormat fmt)
{
    return
        fmt == PIX_FMT_XVMC_MPEG2_VLD;
}

static bool IS_XVMC_PIX_FMT(enum PixelFormat fmt)
{
    return
        fmt == PIX_FMT_XVMC_MPEG2_MC ||
        fmt == PIX_FMT_XVMC_MPEG2_IDCT;
}

static bool IS_VDPAU_PIX_FMT(enum PixelFormat fmt)
{
    return
        fmt == PIX_FMT_VDPAU_H264  ||
        fmt == PIX_FMT_VDPAU_MPEG1 ||
        fmt == PIX_FMT_VDPAU_MPEG2 ||
        fmt == PIX_FMT_VDPAU_MPEG4 ||
        fmt == PIX_FMT_VDPAU_WMV3  ||
        fmt == PIX_FMT_VDPAU_VC1;
}

static enum PixelFormat get_format_xvmc_vld(struct AVCodecContext *avctx,
                                            const enum PixelFormat *fmt)
{
    int i = 0;

    for(i=0; fmt[i]!=PIX_FMT_NONE; i++)
        if (IS_XVMC_VLD_PIX_FMT(fmt[i]))
            break;

    return fmt[i];
}

static enum PixelFormat get_format_xvmc(struct AVCodecContext *avctx,
                                        const enum PixelFormat *fmt)
{
    int i = 0;

    for(i=0; fmt[i]!=PIX_FMT_NONE; i++)
        if (IS_XVMC_PIX_FMT(fmt[i]))
            break;

    return fmt[i];
}

static enum PixelFormat get_format_vdpau(struct AVCodecContext *avctx,
                                         const enum PixelFormat *fmt)
{
    int i = 0;

    for(i=0; fmt[i]!=PIX_FMT_NONE; i++)
        if (IS_VDPAU_PIX_FMT(fmt[i]))
            break;

    return fmt[i];
}

static enum PixelFormat get_format_dxva2(struct AVCodecContext *avctx,
                                         const enum PixelFormat *fmt)
{
    if (!fmt)
        return PIX_FMT_NONE;
    int i = 0;
    for (; fmt[i] != PIX_FMT_NONE ; i++)
        if (PIX_FMT_DXVA2_VLD == fmt[i])
            break;
    return fmt[i];
}

static bool IS_VAAPI_PIX_FMT(enum PixelFormat fmt)
{
    return fmt == PIX_FMT_VAAPI_MOCO ||
           fmt == PIX_FMT_VAAPI_IDCT ||
           fmt == PIX_FMT_VAAPI_VLD;
}

static enum PixelFormat get_format_vaapi(struct AVCodecContext *avctx,
                                         const enum PixelFormat *fmt)
{
    if (!fmt)
        return PIX_FMT_NONE;
    int i = 0;
    for (; fmt[i] != PIX_FMT_NONE ; i++)
        if (IS_VAAPI_PIX_FMT(fmt[i]))
            break;
    return fmt[i];
}

static bool IS_DR1_PIX_FMT(const enum PixelFormat fmt)
{
    switch (fmt)
    {
        case PIX_FMT_YUV420P:
        case PIX_FMT_YUVJ420P:
            return true;
        default:
            return false;
    }
}

void AvFormatDecoder::InitVideoCodec(AVStream *stream, AVCodecContext *enc,
                                     bool selectedStream)
{
    VERBOSE(VB_PLAYBACK, LOC
            <<"InitVideoCodec() "<<enc<<" "
            <<"id("<<ff_codec_id_string(enc->codec_id)
            <<") type ("<<ff_codec_type_string(enc->codec_type)
            <<").");

    if (ringBuffer && ringBuffer->isDVD())
        directrendering = false;

    enc->opaque = (void *)this;
    enc->get_buffer = get_avf_buffer;
    enc->release_buffer = release_avf_buffer;
    enc->draw_horiz_band = NULL;
    enc->slice_flags = 0;

    enc->error_recognition = FF_ER_COMPLIANT;
    enc->workaround_bugs = FF_BUG_AUTODETECT;
    enc->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    enc->idct_algo = FF_IDCT_AUTO;
    enc->debug = 0;
    enc->rate_emu = 0;
    enc->error_rate = 0;

    AVCodec *codec = avcodec_find_decoder(enc->codec_id);
    // look for a vdpau capable codec
    if (codec_is_vdpau(video_codec_id) && !CODEC_IS_VDPAU(codec))
        codec = find_vdpau_decoder(codec, enc->codec_id);

    if (selectedStream)
    {
        directrendering = true;
        if (!gCoreContext->GetNumSetting("DecodeExtraAudio", 0) &&
            !CODEC_IS_HWACCEL(codec, enc))
        {
            SetLowBuffers(false);
        }
    }

    if (CODEC_IS_XVMC(codec))
    {
        enc->flags |= CODEC_FLAG_EMU_EDGE;
        enc->get_buffer = get_avf_buffer_xvmc;
        enc->get_format = (codec->id == CODEC_ID_MPEG2VIDEO_XVMC) ?
                            get_format_xvmc : get_format_xvmc_vld;
        enc->release_buffer = release_avf_buffer_xvmc;
        enc->draw_horiz_band = render_slice_xvmc;
        enc->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else if (CODEC_IS_VDPAU(codec))
    {
        enc->get_buffer      = get_avf_buffer_vdpau;
        enc->get_format      = get_format_vdpau;
        enc->release_buffer  = release_avf_buffer_vdpau;
        enc->draw_horiz_band = render_slice_vdpau;
        enc->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else if (codec && codec->capabilities & CODEC_CAP_DR1)
    {
        enc->flags          |= CODEC_FLAG_EMU_EDGE;
    }
    else
    {
        if (selectedStream)
            directrendering = false;
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Using software scaling to convert pixel format %1 for "
                        "codec %2").arg(enc->pix_fmt)
                .arg(ff_codec_id_string(enc->codec_id)));
    }

    if (special_decode)
    {
        if (special_decode & kAVSpecialDecode_SingleThreaded)
            enc->thread_count = 1;

        enc->flags2 |= CODEC_FLAG2_FAST;

        if ((CODEC_ID_MPEG2VIDEO == codec->id) ||
            (CODEC_ID_MPEG1VIDEO == codec->id))
        {
            if (special_decode & kAVSpecialDecode_FewBlocks)
            {
                uint total_blocks = (enc->height+15) / 16;
                enc->skip_top     = (total_blocks+3) / 4;
                enc->skip_bottom  = (total_blocks+3) / 4;
            }

            if (special_decode & kAVSpecialDecode_LowRes)
                enc->lowres = 2; // 1 = 1/2 size, 2 = 1/4 size
        }
        else if (CODEC_ID_H264 == codec->id)
        {
            if (special_decode & kAVSpecialDecode_NoLoopFilter)
            {
                enc->flags &= ~CODEC_FLAG_LOOP_FILTER;
                enc->skip_loop_filter = AVDISCARD_ALL;
            }
        }

        if (special_decode & kAVSpecialDecode_NoDecode)
        {
            enc->skip_idct = AVDISCARD_ALL;
        }
    }

    if (selectedStream)
    {
        fps = normalized_fps(stream, enc);
        QSize dim    = get_video_dim(*enc);
        int   width  = current_width  = dim.width();
        int   height = current_height = dim.height();
        float aspect = current_aspect = get_aspect(*enc);

        if (!width || !height)
        {
            VERBOSE(VB_PLAYBACK, LOC + "InitVideoCodec "
                    "invalid dimensions, resetting decoder.");
            width  = 640;
            height = 480;
            fps    = 29.97f;
            aspect = 4.0f / 3.0f;
        }

        m_parent->SetVideoParams(width, height, fps,
                                 keyframedist, aspect, kScan_Detect,
                                 dvd_video_codec_changed);
        if (LCD *lcd = LCD::Get())
        {
            LCDVideoFormatSet video_format;

            switch (enc->codec_id)
            {
                case CODEC_ID_H263:
                case CODEC_ID_MPEG4:
                case CODEC_ID_MSMPEG4V1:
                case CODEC_ID_MSMPEG4V2:
                case CODEC_ID_MSMPEG4V3:
                case CODEC_ID_H263P:
                case CODEC_ID_H263I:
                    video_format = VIDEO_DIVX;
                    break;
                case CODEC_ID_WMV1:
                case CODEC_ID_WMV2:
                    video_format = VIDEO_WMV;
                    break;
                case CODEC_ID_XVID:
                    video_format = VIDEO_XVID;
                    break;
                default:
                    video_format = VIDEO_MPG;
                    break;
            }

            lcd->setVideoFormatLEDs(video_format, true);

            if(height >= 720)
                lcd->setVariousLEDs(VARIOUS_HDTV, true);
            else
                lcd->setVariousLEDs(VARIOUS_HDTV, false);
        }
    }
}

#ifdef USING_XVMC
static int xvmc_pixel_format(enum PixelFormat pix_fmt)
{
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

// CC Parity checking
// taken from xine-lib libspucc

static int cc608_parity(uint8_t byte)
{
    int ones = 0;

    for (int i = 0; i < 7; i++)
    {
        if (byte & (1 << i))
            ones++;
    }

    return ones & 1;
}

// CC Parity checking
// taken from xine-lib libspucc

static void cc608_build_parity_table(int *parity_table)
{
    uint8_t byte;
    int parity_v;
    for (byte = 0; byte <= 127; byte++)
    {
        parity_v = cc608_parity(byte);
        /* CC uses odd parity (i.e., # of 1's in byte is odd.) */
        parity_table[byte] = parity_v;
        parity_table[byte | 0x80] = !parity_v;
    }
}

// CC Parity checking
// taken from xine-lib libspucc

static int cc608_good_parity(const int *parity_table, uint16_t data)
{
    int ret = parity_table[data & 0xff] && parity_table[(data & 0xff00) >> 8];
    if (!ret)
    {
        VERBOSE(VB_VBI, LOC_ERR + QString("VBI: Bad parity in EIA-608 data (%1)")
                .arg(data,0,16));
    }
    return ret;
}

void AvFormatDecoder::ScanATSCCaptionStreams(int av_index)
{
    memset(ccX08_in_pmt, 0, sizeof(ccX08_in_pmt));
    pmt_tracks.clear();
    pmt_track_types.clear();

    // Figure out languages of ATSC captions
    if (!ic->cur_pmt_sect)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "ScanATSCCaptionStreams() called with no PMT");
        return;
    }

    const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
    const PSIPTable psip(pes);
    const ProgramMapTable pmt(psip);

    uint i;
    for (i = 0; i < pmt.StreamCount(); i++)
    {
        // MythTV remaps OpenCable Video to normal video during recording
        // so "dvb" is the safest choice for system info type, since this
        // will ignore other uses of the same stream id in DVB countries.
        if (pmt.IsVideo(i, "dvb"))
            break;
    }

    if (!pmt.IsVideo(i, "dvb"))
        return;

    const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
        pmt.StreamInfo(i), pmt.StreamInfoLength(i),
        DescriptorID::caption_service);

    for (uint j = 0; j < desc_list.size(); j++)
    {
        const CaptionServiceDescriptor csd(desc_list[j]);
        for (uint k = 0; k < csd.ServicesCount(); k++)
        {
            int lang = csd.CanonicalLanguageKey(k);
            int type = csd.Type(k) ? 1 : 0;
            if (type)
            {
                StreamInfo si(av_index, lang, 0/*lang_idx*/,
                              csd.CaptionServiceNumber(k),
                              csd.EasyReader(k),
                              csd.WideAspectRatio(k));
                uint key = csd.CaptionServiceNumber(k) + 4;
                ccX08_in_pmt[key] = true;
                pmt_tracks.push_back(si);
                pmt_track_types.push_back(kTrackTypeCC708);
            }
            else
            {
                int line21 = csd.Line21Field(k) ? 3 : 1;
                StreamInfo si(av_index, lang, 0/*lang_idx*/, line21, 0);
                ccX08_in_pmt[line21-1] = true;
                pmt_tracks.push_back(si);
                pmt_track_types.push_back(kTrackTypeCC608);
            }
        }
    }
}

void AvFormatDecoder::UpdateATSCCaptionTracks(void)
{
    tracks[kTrackTypeCC608].clear();
    tracks[kTrackTypeCC708].clear();
    memset(ccX08_in_tracks, 0, sizeof(ccX08_in_tracks));

    uint pidx = 0, sidx = 0;
    map<int,uint> lang_cc_cnt[2];
    while (true)
    {
        bool pofr = pidx >= (uint)pmt_tracks.size();
        bool sofr = sidx >= (uint)stream_tracks.size();
        if (pofr && sofr)
            break;

        // choose lowest available next..
        // stream_id's of 608 and 708 streams alias, but this
        // is ok as we just want each list to be ordered.
        StreamInfo const *si = NULL;
        int type = 0; // 0 if 608, 1 if 708
        bool isp = true; // if true use pmt_tracks next, else stream_tracks

        if (pofr && !sofr)
            isp = false;
        else if (!pofr && sofr)
            isp = true;
        else if (stream_tracks[sidx] < pmt_tracks[pidx])
            isp = false;

        if (isp)
        {
            si = &pmt_tracks[pidx];
            type = kTrackTypeCC708 == pmt_track_types[pidx] ? 1 : 0;
            pidx++;
        }
        else
        {
            si = &stream_tracks[sidx];
            type = kTrackTypeCC708 == stream_track_types[sidx] ? 1 : 0;
            sidx++;
        }

        StreamInfo nsi(*si);
        int lang_indx = lang_cc_cnt[type][nsi.language];
        lang_cc_cnt[type][nsi.language]++;
        nsi.language_index = lang_indx;
        tracks[(type) ? kTrackTypeCC708 : kTrackTypeCC608].push_back(nsi);
        int key = (int)nsi.stream_id + ((type) ? 4 : -1);
        if (key < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                    "Programmer Error in_tracks key too small");
        }
        else
        {
            ccX08_in_tracks[key] = true;
        }
        VERBOSE(VB_PLAYBACK, LOC + QString(
                    "%1 caption service #%2 is in the %3 language.")
                .arg((type) ? "EIA-708" : "EIA-608")
                .arg(nsi.stream_id)
                .arg(iso639_key_toName(nsi.language)));
    }
}

void AvFormatDecoder::ScanTeletextCaptions(int av_index)
{
    // ScanStreams() calls tracks[kTrackTypeTeletextCaptions].clear()
    if (!ic->cur_pmt_sect || tracks[kTrackTypeTeletextCaptions].size())
        return;

    const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
    const PSIPTable psip(pes);
    const ProgramMapTable pmt(psip);

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        if (pmt.StreamType(i) != 6)
            continue;

        const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::teletext);

        for (uint j = 0; j < desc_list.size(); j++)
        {
            const TeletextDescriptor td(desc_list[j]);
            for (uint k = 0; k < td.StreamCount(); k++)
            {
                int type = td.TeletextType(k);
                int language = td.CanonicalLanguageKey(k);
                int magazine = td.TeletextMagazineNum(k)?:8;
                int pagenum  = td.TeletextPageNum(k);
                int lang_idx = (magazine << 8) | pagenum;
                StreamInfo si(av_index, language, lang_idx, 0, 0);
                if (type == 2 || type == 1)
                {
                    TrackType track = (type == 2) ? kTrackTypeTeletextCaptions :
                                                    kTrackTypeTeletextMenu;
                    tracks[track].push_back(si);
                    VERBOSE(VB_PLAYBACK, LOC + QString(
                                "Teletext stream #%1 (%2) is in the %3 language"
                                " on page %4 %5.")
                            .arg(k).arg((type == 2) ? "Caption" : "Menu")
                            .arg(iso639_key_toName(language))
                            .arg(magazine).arg(pagenum));
                }
            }
        }

        // Assume there is only one multiplexed teletext stream in PMT..
        if (tracks[kTrackTypeTeletextCaptions].size())
            break;
    }
}

void AvFormatDecoder::ScanRawTextCaptions(int av_stream_index)
{
    AVMetadataTag *metatag = av_metadata_get(ic->streams[av_stream_index]->metadata,
                                             "language", NULL, 0);
    int lang = metatag ? get_canonical_lang(metatag->value) :
                         iso639_str3_to_key("und");
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Text Subtitle track #%1 is A/V stream #%2 "
                    "and is in the %3 language(%4).")
                    .arg(tracks[kTrackTypeRawText].size()).arg(av_stream_index)
                    .arg(iso639_key_toName(lang)).arg(lang));
    StreamInfo si(av_stream_index, lang, 0, 0, 0);
    tracks[kTrackTypeRawText].push_back(si);
}

/** \fn AvFormatDecoder::ScanDSMCCStreams(void)
 *  \brief Check to see whether there is a Network Boot Ifo sub-descriptor in the PMT which
 *         requires the MHEG application to reboot.
 */
void AvFormatDecoder::ScanDSMCCStreams(void)
{
    if (!ic || !ic->cur_pmt_sect)
        return;

    if (!itv && ! (itv = m_parent->GetInteractiveTV()))
        return;

    const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
    const PSIPTable psip(pes);
    const ProgramMapTable pmt(psip);

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        if (! StreamID::IsObjectCarousel(pmt.StreamType(i)))
            continue;

        const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::data_broadcast_id);

        for (uint j = 0; j < desc_list.size(); j++)
        {
            const unsigned char *desc = desc_list[j];
            desc++; // Skip tag
            uint length = *desc++;
            const unsigned char *endDesc = desc+length;
            uint dataBroadcastId = desc[0]<<8 | desc[1];
            if (dataBroadcastId != 0x0106) // ETSI/UK Profile
                continue;
            desc += 2; // Skip data ID
            while (desc != endDesc)
            {
                uint appTypeCode = desc[0]<<8 | desc[1];
                desc += 3; // Skip app type code and boot priority hint
                uint appSpecDataLen = *desc++;
#ifdef USING_MHEG
                if (appTypeCode == 0x101) // UK MHEG profile
                {
                    const unsigned char *subDescEnd = desc + appSpecDataLen;
                    while (desc < subDescEnd)
                    {
                        uint sub_desc_tag = *desc++;
                        uint sub_desc_len = *desc++;
                        // Network boot info sub-descriptor.
                        if (sub_desc_tag == 1)
                            itv->SetNetBootInfo(desc, sub_desc_len);
                        desc += sub_desc_len;
                    }
                }
                else
#else
                (void) appTypeCode;
#endif // USING_MHEG
                {
                    desc += appSpecDataLen;
                }
            }
        }
    }
}

int AvFormatDecoder::ScanStreams(bool novideo)
{
    int scanerror = 0;
    bitrate = 0;
    fps = 0;

    tracks[kTrackTypeAudio].clear();
    tracks[kTrackTypeSubtitle].clear();
    tracks[kTrackTypeTeletextCaptions].clear();
    tracks[kTrackTypeTeletextMenu].clear();
    tracks[kTrackTypeRawText].clear();
    tracks[kTrackTypeVideo].clear();
    selectedTrack[kTrackTypeVideo].av_stream_index = -1;

    map<int,uint> lang_sub_cnt;
    uint subtitleStreamCount = 0;
    map<int,uint> lang_aud_cnt;
    uint audioStreamCount = 0;

    if (ringBuffer && ringBuffer->isDVD() &&
        ringBuffer->DVD()->AudioStreamsChanged())
    {
        ringBuffer->DVD()->AudioStreamsChanged(false);
        RemoveAudioStreams();
    }

    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = ic->streams[i]->codec;
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Stream #%1, has id 0x%2 codec id %3, "
                        "type %4, bitrate %5 at ")
                .arg(i).arg((int)ic->streams[i]->id, 0, 16)
                .arg(ff_codec_id_string(enc->codec_id))
                .arg(ff_codec_type_string(enc->codec_type))
                .arg(enc->bit_rate)
                <<((void*)ic->streams[i]));

        switch (enc->codec_type)
        {
            case CODEC_TYPE_VIDEO:
            {
                //assert(enc->codec_id);
                if (!enc->codec_id)
                {
                    VERBOSE(VB_IMPORTANT,
                            LOC + QString("Stream #%1 has an unknown video "
                                          "codec id, skipping.").arg(i));
                    continue;
                }

                // HACK -- begin
                // ffmpeg is unable to compute H.264 bitrates in mpegts?
                if (CODEC_IS_H264(enc->codec_id) && enc->bit_rate == 0)
                    enc->bit_rate = 500000;
                // HACK -- end

                StreamInfo si(i, 0, 0, 0, 0);
                tracks[kTrackTypeVideo].push_back(si);
                bitrate += enc->bit_rate;
                if (novideo)
                    break;

                delete private_dec;
                private_dec = NULL;
                m_h264_parser->Reset();

                QSize dim = get_video_dim(*enc);
                uint width  = max(dim.width(),  16);
                uint height = max(dim.height(), 16);
                QString dec = "ffmpeg";
                uint thread_count = 1;

                if (!is_db_ignored)
                {
                    VideoDisplayProfile vdp;
                    vdp.SetInput(QSize(width, height));
                    dec = vdp.GetDecoder();
                    thread_count = vdp.GetMaxCPUs();
                    bool skip_loop_filter = vdp.IsSkipLoopEnabled();
                    if  (!skip_loop_filter)
                    {
                        enc->skip_loop_filter = AVDISCARD_NONKEY;
                    }
                }

                bool handled = false;
                if (!using_null_videoout && mpeg_version(enc->codec_id))
                {
#if defined(USING_VDPAU) || defined(USING_XVMC)
                    // HACK -- begin
                    // Force MPEG2 decoder on MPEG1 streams.
                    // Needed for broken transmitters which mark
                    // MPEG2 streams as MPEG1 streams, and should
                    // be harmless for unbroken ones.
                    if (CODEC_ID_MPEG1VIDEO == enc->codec_id)
                        enc->codec_id = CODEC_ID_MPEG2VIDEO;
                    // HACK -- end
#endif // USING_XVMC || USING_VDPAU
#ifdef USING_VDPAU
                    MythCodecID vdpau_mcid;
                    vdpau_mcid = VideoOutputVDPAU::GetBestSupportedCodec(
                        width, height,
                        mpeg_version(enc->codec_id), no_hardware_decoders);

                    if (vdpau_mcid >= video_codec_id)
                    {
                        enc->codec_id = (CodecID) myth2av_codecid(vdpau_mcid);
                        video_codec_id = vdpau_mcid;
                        handled = true;
                    }
#endif // USING_VDPAU
#ifdef USING_XVMC

                    bool force_xv = no_hardware_decoders;
                    if (ringBuffer && ringBuffer->isDVD())
                    {
                        if (dec.left(4) == "xvmc")
                            dvd_xvmc_enabled = true;

                        if (ringBuffer->IsDVD())
                        {
                            force_xv = true;
                            enc->pix_fmt = PIX_FMT_YUV420P;
                        }
                    }

                    MythCodecID mcid;
                    mcid = VideoOutputXv::GetBestSupportedCodec(
                        /* disp dim     */ width, height,
                        /* osd dim      */ /*enc->width*/ 0, /*enc->height*/ 0,
                        /* mpeg type    */ mpeg_version(enc->codec_id),
                        /* xvmc pix fmt */ xvmc_pixel_format(enc->pix_fmt),
                        /* test surface */ codec_is_std(video_codec_id),
                        /* force_xv     */ force_xv);

                    if (mcid >= video_codec_id)
                    {
                        bool vcd, idct, mc, vdpau;
                        enc->codec_id = (CodecID)
                            myth2av_codecid(mcid, vcd, idct, mc, vdpau);

                        if (ringBuffer && ringBuffer->isDVD() &&
                            (mcid == video_codec_id) &&
                            dvd_video_codec_changed)
                        {
                            dvd_video_codec_changed = false;
                            dvd_xvmc_enabled = false;
                        }

                        video_codec_id = mcid;
                        if (!force_xv && codec_is_xvmc_std(mcid))
                        {
                            enc->pix_fmt = (idct) ?
                                PIX_FMT_XVMC_MPEG2_IDCT :
                                PIX_FMT_XVMC_MPEG2_MC;
                        }
                        handled = true;
                    }
#endif // USING_XVMC
                }

                if (!handled)
                {
                    if (CODEC_IS_H264(enc->codec_id))
                        video_codec_id = kCodec_H264;
                    else
                        video_codec_id = kCodec_MPEG2; // default to MPEG2
                }

                if (enc->codec)
                {
                    VERBOSE(VB_IMPORTANT, LOC
                            <<"Warning, video codec "<<enc<<" "
                            <<"id("<<ff_codec_id_string(enc->codec_id)
                            <<") type ("<<ff_codec_type_string(enc->codec_type)
                            <<") already open.");
                }

                // Set the default stream to the stream
                // that is found first in the PMT
                if (selectedTrack[kTrackTypeVideo].av_stream_index < 0)
                    selectedTrack[kTrackTypeVideo] = si;

                if (allow_private_decoders &&
                   (selectedTrack[kTrackTypeVideo].av_stream_index == (int) i))
                {
                    private_dec = PrivateDecoder::Create(
                                            dec, no_hardware_decoders, enc);
                    if (private_dec)
                        thread_count = 1;
                }

                if (!codec_is_std(video_codec_id))
                    thread_count = 1;

                VERBOSE(VB_PLAYBACK, LOC + QString("Using %1 CPUs for decoding")
                        .arg(HAVE_THREADS ? thread_count : 1));

                if (HAVE_THREADS && thread_count > 1)
                {
                    if (enc->thread_count > 1)
                        avcodec_thread_free(enc);
                    avcodec_thread_init(enc, thread_count);
                }

                InitVideoCodec(ic->streams[i], enc,
                    selectedTrack[kTrackTypeVideo].av_stream_index == (int) i);

                ScanATSCCaptionStreams(i);
                UpdateATSCCaptionTracks();

                VERBOSE(VB_PLAYBACK, LOC +
                        QString("Using %1 for video decoding")
                        .arg(GetCodecDecoderName()));

                break;
            }
            case CODEC_TYPE_AUDIO:
            {
                if (enc->codec)
                {
                    VERBOSE(VB_IMPORTANT, LOC
                            <<"Warning, audio codec "<<enc
                            <<" id("<<ff_codec_id_string(enc->codec_id)
                            <<") type ("<<ff_codec_type_string(enc->codec_type)
                            <<") already open, leaving it alone.");
                }
                //assert(enc->codec_id);
                VERBOSE(VB_GENERAL, LOC + QString("codec %1 has %2 channels")
                        .arg(ff_codec_id_string(enc->codec_id))
                        .arg(enc->channels));

#if 0
                // HACK MULTICHANNEL DTS passthru disabled for multichannel,
                // dont know how to handle this
                // HACK BEGIN REALLY UGLY HACK FOR DTS PASSTHRU
                if (enc->codec_id == CODEC_ID_DTS)
                {
                    enc->sample_rate = 48000;
                    enc->channels = 2;
                    // enc->bit_rate = what??;
                }
                // HACK END REALLY UGLY HACK FOR DTS PASSTHRU
#endif

                bitrate += enc->bit_rate;
                break;
            }
            case CODEC_TYPE_SUBTITLE:
            {
                if (enc->codec_id == CODEC_ID_DVB_TELETEXT)
                    ScanTeletextCaptions(i);
                if (enc->codec_id == CODEC_ID_TEXT)
                    ScanRawTextCaptions(i);
                bitrate += enc->bit_rate;

                VERBOSE(VB_PLAYBACK, LOC + QString("subtitle codec (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
            case CODEC_TYPE_DATA:
            {
                ScanTeletextCaptions(i);
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, LOC + QString("data codec (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
            default:
            {
                bitrate += enc->bit_rate;
                VERBOSE(VB_PLAYBACK, LOC + QString("Unknown codec type (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
        }

        if (enc->codec_type != CODEC_TYPE_AUDIO &&
            enc->codec_type != CODEC_TYPE_VIDEO &&
            enc->codec_type != CODEC_TYPE_SUBTITLE)
            continue;

        // skip DVB teletext, text and SSA subs, there is no libavcodec decoder
        if (enc->codec_type == CODEC_TYPE_SUBTITLE &&
           (enc->codec_id   == CODEC_ID_DVB_TELETEXT ||
            enc->codec_id   == CODEC_ID_TEXT ||
            enc->codec_id   == CODEC_ID_SSA))
            continue;

        VERBOSE(VB_PLAYBACK, LOC + QString("Looking for decoder for %1")
                .arg(ff_codec_id_string(enc->codec_id)));

        if (enc->codec_id == CODEC_ID_PROBE)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Probing of stream #%1 unsuccesful, "
                            "ignoring.").arg(i));
            continue;
        }

        AVCodec *codec = avcodec_find_decoder(enc->codec_id);
        if (!codec)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Could not find decoder for "
                            "codec (%1), ignoring.")
                    .arg(ff_codec_id_string(enc->codec_id)));

            // Nigel's bogus codec-debug. Dump the list of codecs & decoders,
            // and have one last attempt to find a decoder. This is usually
            // only caused by build problems, where libavcodec needs a rebuild
            if (VERBOSE_LEVEL_CHECK(VB_LIBAV))
            {
                AVCodec *p = av_codec_next(NULL);
                int      i = 1;
                while (p)
                {
                    if (p->name[0] != '\0')  printf("Codec %s:", p->name);
                    else                     printf("Codec %d, null name,", i);
                    if (p->decode == NULL)   puts("decoder is null");

                    if (p->id == enc->codec_id)
                    {   codec = p; break;    }

                    printf("Codec 0x%x != 0x%x\n", p->id, enc->codec_id);
                    p = av_codec_next(p);
                    ++i;
                }
            }
            if (!codec)
                continue;
        }
        // select vdpau capable decoder if needed
        else if (enc->codec_type == CODEC_TYPE_VIDEO &&
                 codec_is_vdpau(video_codec_id) && !CODEC_IS_VDPAU(codec))
        {
            codec = find_vdpau_decoder(codec, enc->codec_id);
        }

        if (!enc->codec)
        {
            QMutexLocker locker(avcodeclock);

            int open_val = avcodec_open(enc, codec);
            if (open_val < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR
                        <<"Could not open codec "<<enc<<", "
                        <<"id("<<ff_codec_id_string(enc->codec_id)<<") "
                        <<"type("<<ff_codec_type_string(enc->codec_type)<<") "
                        <<"aborting. reason "<<open_val);
                //av_close_input_file(ic); // causes segfault
                ic = NULL;
                scanerror = -1;
                break;
            }
            else
            {
                VERBOSE(VB_GENERAL, LOC + "Opened codec "<<enc<<", "
                        <<"id("<<ff_codec_id_string(enc->codec_id)<<") "
                        <<"type("<<ff_codec_type_string(enc->codec_type)<<")");
            }
        }

        if (enc->codec_type == CODEC_TYPE_SUBTITLE)
        {
            int lang;
            if (ringBuffer && ringBuffer->isBD())
                lang = ringBuffer->BD()->GetSubtitleLanguage(subtitleStreamCount);
            else
            {
                AVMetadataTag *metatag = av_metadata_get(ic->streams[i]->metadata,
                                                        "language", NULL, 0);
                lang = metatag ? get_canonical_lang(metatag->value) : iso639_str3_to_key("und");
            }

            int lang_indx = lang_sub_cnt[lang]++;
            subtitleStreamCount++;

            tracks[kTrackTypeSubtitle].push_back(
                StreamInfo(i, lang, lang_indx, ic->streams[i]->id, 0));

            VERBOSE(VB_PLAYBACK, LOC + QString(
                        "Subtitle track #%1 is A/V stream #%2 "
                        "and is in the %3 language(%4).")
                    .arg(tracks[kTrackTypeSubtitle].size()).arg(i)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }

        if (enc->codec_type == CODEC_TYPE_AUDIO)
        {
            int lang;
            if (ringBuffer && ringBuffer->isDVD())
                lang = ringBuffer->DVD()->GetAudioLanguage(
                    ringBuffer->DVD()->GetAudioTrackNum(ic->streams[i]->id));
            else if (ringBuffer && ringBuffer->isBD())
                lang = ringBuffer->BD()->GetAudioLanguage(audioStreamCount);
            else
            {
                AVMetadataTag *metatag = av_metadata_get(ic->streams[i]->metadata,
                                                         "language", NULL, 0);
                lang = metatag ? get_canonical_lang(metatag->value) : iso639_str3_to_key("und");
            }

            int channels  = ic->streams[i]->codec->channels;
            int lang_indx = lang_aud_cnt[lang]++;
            audioStreamCount++;

            if (ic->streams[i]->codec->avcodec_dual_language)
            {
                tracks[kTrackTypeAudio].push_back(
                    StreamInfo(i, lang, lang_indx, ic->streams[i]->id, channels, false));
                lang_indx = lang_aud_cnt[lang]++;
                tracks[kTrackTypeAudio].push_back(
                    StreamInfo(i, lang, lang_indx, ic->streams[i]->id, channels, true));
            }
            else
            {
                int logical_stream_id;
                if (ringBuffer && ringBuffer->isDVD())
                    logical_stream_id = ringBuffer->DVD()->GetAudioTrackNum(ic->streams[i]->id);
                else
                    logical_stream_id = ic->streams[i]->id;

                tracks[kTrackTypeAudio].push_back(
                    StreamInfo(i, lang, lang_indx, logical_stream_id, channels));
            }

            VERBOSE(VB_AUDIO, LOC + QString(
                        "Audio Track #%1 is A/V stream #%2 "
                        "and has %3 channels in the %4 language(%5).")
                    .arg(tracks[kTrackTypeAudio].size()).arg(i)
                    .arg(enc->channels)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }
    }

    if (bitrate > 0)
    {
        bitrate = (bitrate + 999) / 1000;
        if (ringBuffer)
            ringBuffer->UpdateRawBitrate(bitrate);
    }

    if (ringBuffer && ringBuffer->isDVD())
    {
        if (tracks[kTrackTypeAudio].size() > 1)
        {
            stable_sort(tracks[kTrackTypeAudio].begin(),
                        tracks[kTrackTypeAudio].end());
            sinfo_vec_t::iterator it = tracks[kTrackTypeAudio].begin();
            for (; it != tracks[kTrackTypeAudio].end(); ++it)
            {
                VERBOSE(VB_PLAYBACK, LOC +
                            QString("DVD Audio Track Map "
                                    "Stream id #%1, MPEG stream %2")
                                    .arg(it->stream_id)
                                    .arg(ic->streams[it->av_stream_index]->id));
            }
            int trackNo = ringBuffer->DVD()->GetTrack(kTrackTypeAudio);
            if (trackNo >= (int)GetTrackCount(kTrackTypeAudio))
                trackNo = GetTrackCount(kTrackTypeAudio) - 1;
            SetTrack(kTrackTypeAudio, trackNo);
        }
        if (tracks[kTrackTypeSubtitle].size() > 0)
        {
            stable_sort(tracks[kTrackTypeSubtitle].begin(),
                        tracks[kTrackTypeSubtitle].end());
            sinfo_vec_t::iterator it = tracks[kTrackTypeSubtitle].begin();
            for(; it != tracks[kTrackTypeSubtitle].end(); ++it)
            {
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("DVD Subtitle Track Map "
                                "Stream id #%1 ")
                                .arg(it->stream_id));
                }
            stable_sort(tracks[kTrackTypeSubtitle].begin(),
                        tracks[kTrackTypeSubtitle].end());
            int trackNo = ringBuffer->DVD()->GetTrack(kTrackTypeSubtitle);
            uint captionmode = m_parent->GetCaptionMode();
            int trackcount = (int)GetTrackCount(kTrackTypeSubtitle);
            if (captionmode == kDisplayAVSubtitle &&
                (trackNo < 0 || trackNo >= trackcount))
            {
                m_parent->EnableSubtitles(false);
            }
            else if (trackNo >= 0 && trackNo < trackcount &&
                    !ringBuffer->InDVDMenuOrStillFrame())
            {
                    SetTrack(kTrackTypeSubtitle, trackNo);
                    m_parent->EnableSubtitles(true);
            }
        }
    }

    // Select a new track at the next opportunity.
    ResetTracks();

    // We have to do this here to avoid the NVP getting stuck
    // waiting on audio.
    if (m_audio->HasAudioIn() && tracks[kTrackTypeAudio].empty())
    {
        m_audio->SetAudioParams(FORMAT_NONE, -1, -1, CODEC_ID_NONE, -1, false);
        m_audio->ReinitAudio();
        if (ringBuffer && ringBuffer->isDVD())
            audioIn = AudioInfo();
    }

    // if we don't have a video stream we still need to make sure some
    // video params are set properly
    if (selectedTrack[kTrackTypeVideo].av_stream_index == -1)
    {
        QString tvformat = gCoreContext->GetSetting("TVFormat").toLower();
        if (tvformat == "ntsc" || tvformat == "ntsc-jp" ||
            tvformat == "pal-m" || tvformat == "atsc")
        {
            fps = 29.97;
            m_parent->SetVideoParams(-1, -1, 29.97, 1);
        }
        else
        {
            fps = 25.0;
            m_parent->SetVideoParams(-1, -1, 25.0, 1);
        }
    }

    if (m_parent->IsErrored())
        scanerror = -1;

    ScanDSMCCStreams();

    return scanerror;
}

/**
 *  \brief Reacts to DUAL/STEREO changes on the fly and fix streams.
 *
 *  This function should be called when a switch between dual and
 *  stereo mpeg audio is detected. Such changes can and will happen at
 *  any time.
 *
 *  After this method returns, a new audio stream should be selected
 *  using AvFormatDecoder::autoSelectSubtitleTrack().
 *
 *  \param streamIndex av_stream_index of the stream that has changed
 */
void AvFormatDecoder::SetupAudioStreamSubIndexes(int streamIndex)
{
    QMutexLocker locker(avcodeclock);

    // Find the position of the streaminfo in tracks[kTrackTypeAudio]
    sinfo_vec_t::iterator current = tracks[kTrackTypeAudio].begin();
    for (; current != tracks[kTrackTypeAudio].end(); ++current)
    {
        if (current->av_stream_index == streamIndex)
            break;
    }

    if (current == tracks[kTrackTypeAudio].end())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Invalid stream index passed to "
                "SetupAudioStreamSubIndexes: "<<streamIndex);

        return;
    }

    // Remove the extra substream or duplicate the current substream
    sinfo_vec_t::iterator next = current + 1;
    if (current->av_substream_index == -1)
    {
        // Split stream in two (Language I + Language II)
        StreamInfo lang1 = *current;
        StreamInfo lang2 = *current;
        lang1.av_substream_index = 0;
        lang2.av_substream_index = 1;
        *current = lang1;
        tracks[kTrackTypeAudio].insert(next, lang2);
        return;
    }

    if ((next == tracks[kTrackTypeAudio].end()) ||
        (next->av_stream_index != streamIndex))
    {
        QString msg = QString(
            "Expected substream 1 (Language I) of stream %1\n\t\t\t"
            "following substream 0, found end of list or another stream.")
            .arg(streamIndex);

        VERBOSE(VB_IMPORTANT, LOC_WARN + msg);

        return;
    }

    // Remove extra stream info
    StreamInfo stream = *current;
    stream.av_substream_index = -1;
    *current = stream;
    tracks[kTrackTypeAudio].erase(next);
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    if (!IS_DR1_PIX_FMT(c->pix_fmt))
    {
        nd->directrendering = false;
        return avcodec_default_get_buffer(c, pic);
    }
    nd->directrendering = true;

    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame(true);

    if (!frame)
        return -1;

    for (int i = 0; i < 3; i++)
    {
        pic->data[i]     = frame->buf + frame->offsets[i];
        pic->linesize[i] = frame->pitches[i];
    }

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

/** \brief remove audio streams from the context
 * used by dvd code during title transitions to remove
 * stale audio streams
 */
void AvFormatDecoder::RemoveAudioStreams()
{
    if (!m_audio->HasAudioIn())
        return;

    QMutexLocker locker(avcodeclock);
    for (uint i = 0; i < ic->nb_streams;)
    {
        AVStream *st = ic->streams[i];
        if (st->codec->codec_type == CODEC_TYPE_AUDIO)
        {
            av_remove_stream(ic, st->id, 0);
            i--;
        }
        else
            i++;
    }
}

void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;

    if (pic->type == FF_BUFFER_TYPE_INTERNAL)
    {
        avcodec_default_release_buffer(c, pic);
        return;
    }

    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    if (nd && nd->GetPlayer() && nd->GetPlayer()->getVideoOutput())
        nd->GetPlayer()->getVideoOutput()->DeLimboFrame((VideoFrame*)pic->opaque);

    assert(pic->type == FF_BUFFER_TYPE_USER);

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

int get_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame(false);

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
    struct xvmc_pix_fmt *render = (struct xvmc_pix_fmt *)frame->buf;

    render->state = AV_XVMC_STATE_PREDICTION;
    render->picture_structure = 0;
    render->flags = 0;
    render->start_mv_blocks_num = 0;
    render->filled_mv_blocks_num = 0;
    render->next_free_data_block_num = 0;
#endif

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

void release_avf_buffer_xvmc(struct AVCodecContext *c, AVFrame *pic)
{
    assert(pic->type == FF_BUFFER_TYPE_USER);

#ifdef USING_XVMC
    struct xvmc_pix_fmt *render = (struct xvmc_pix_fmt *)pic->data[2];
    render->state &= ~AV_XVMC_STATE_PREDICTION;
#endif

    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    if (nd && nd->GetPlayer() && nd->GetPlayer()->getVideoOutput())
        nd->GetPlayer()->getVideoOutput()->DeLimboFrame((VideoFrame*)pic->opaque);

    for (uint i = 0; i < 4; i++)
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
        nd->GetPlayer()->DrawSlice(frame, 0, y, width, height);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "render_slice_xvmc called with bad avctx or src");
    }
}

int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame(false);

    pic->data[0] = frame->buf;
    pic->data[1] = frame->priv[0];
    pic->data[2] = frame->priv[1];

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    frame->pix_fmt = c->pix_fmt;

#ifdef USING_VDPAU
    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    render->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;
#endif

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

void release_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic)
{
    assert(pic->type == FF_BUFFER_TYPE_USER);

#ifdef USING_VDPAU
    struct vdpau_render_state *render = (struct vdpau_render_state *)pic->data[0];
    render->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;
#endif

    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    if (nd && nd->GetPlayer() && nd->GetPlayer()->getVideoOutput())
        nd->GetPlayer()->getVideoOutput()->DeLimboFrame((VideoFrame*)pic->opaque);

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

void render_slice_vdpau(struct AVCodecContext *s, const AVFrame *src,
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
        nd->GetPlayer()->DrawSlice(frame, 0, y, width, height);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "render_slice_vdpau called with bad avctx or src");
    }
}

void AvFormatDecoder::DecodeDTVCC(const uint8_t *buf, uint len)
{
    if (!len)
        return;

    // closed caption data
    //cc_data() {
    // reserved                1 0.0   1
    // process_cc_data_flag    1 0.1   bslbf
    bool process_cc_data = buf[0] & 0x40;
    if (!process_cc_data)
        return; // early exit if process_cc_data_flag false

    // additional_data_flag    1 0.2   bslbf
    //bool additional_data = buf[0] & 0x20;
    // cc_count                5 0.3   uimsbf
    uint cc_count = buf[0] & 0x1f;
    // em_data                 8 1.0

    if (len < 2+(3*cc_count))
        return;

    bool had_608 = false, had_708 = false;
    for (uint cur = 0; cur < cc_count; cur++)
    {
        uint cc_code  = buf[2+(cur*3)];
        bool cc_valid = cc_code & 0x04;
        if (!cc_valid)
            continue;

        uint data1    = buf[3+(cur*3)];
        uint data2    = buf[4+(cur*3)];
        uint data     = (data2 << 8) | data1;
        uint cc_type  = cc_code & 0x03;

        if (cc_type <= 0x1) // EIA-608 field-1/2
        {
            if (cc608_good_parity(cc608_parity_table, data))
            {
                had_608 = true;
                ccd608->FormatCCField(lastccptsu / 1000, cc_type, data);
            }
        }
        else
        {
            had_708 = true;
            ccd708->decode_cc_data(cc_type, data1, data2);
        }
    }
    UpdateCaptionTracksFromStreams(had_608, had_708);
}

void AvFormatDecoder::UpdateCaptionTracksFromStreams(
    bool check_608, bool check_708)
{
    bool need_change_608 = false;
    bool seen_608[4];
    if (check_608)
    {
        ccd608->GetServices(15/*seconds*/, seen_608);
        for (uint i = 0; i < 4; i++)
        {
            need_change_608 |= (seen_608[i] && !ccX08_in_tracks[i]) ||
                (!seen_608[i] && ccX08_in_tracks[i] && !ccX08_in_pmt[i]);
        }
    }

    bool need_change_708 = false;
    bool seen_708[64];
    if (check_708 || need_change_608)
    {
        ccd708->services(15/*seconds*/, seen_708);
        for (uint i = 1; i < 64 && !need_change_608 && !need_change_708; i++)
        {
            need_change_708 |= (seen_708[i] && !ccX08_in_tracks[i+4]) ||
                (!seen_708[i] && ccX08_in_tracks[i+4] && !ccX08_in_pmt[i+4]);
        }
        if (need_change_708 && !check_608)
            ccd608->GetServices(15/*seconds*/, seen_608);
    }

    if (!need_change_608 && !need_change_708)
        return;

    ScanATSCCaptionStreams(selectedTrack[kTrackTypeVideo].av_stream_index);

    stream_tracks.clear();
    stream_track_types.clear();
    int av_index = selectedTrack[kTrackTypeVideo].av_stream_index;
    int lang = iso639_str3_to_key("und");
    for (uint i = 0; i < 4; i++)
    {
        if (seen_608[i])
        {
            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i+1, false/*easy*/, false/*wide*/);
            stream_tracks.push_back(si);
            stream_track_types.push_back(kTrackTypeCC608);
        }
    }
    for (uint i = 1; i < 64; i++)
    {
        if (seen_708[i] && !ccX08_in_pmt[i+4])
        {
            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i, false/*easy*/, true/*wide*/);
            stream_tracks.push_back(si);
            stream_track_types.push_back(kTrackTypeCC708);
        }
    }
    UpdateATSCCaptionTracks();
}

void AvFormatDecoder::HandleGopStart(
    AVPacket *pkt, bool can_reliably_parse_keyframes)
{
    if (prevgoppos != 0 && keyframedist != 1)
    {
        int tempKeyFrameDist = framesRead - 1 - prevgoppos;
        bool reset_kfd = false;

        if (!gopset || livetv) // gopset: we've seen 2 keyframes
        {
            VERBOSE(VB_PLAYBACK, LOC + "HandleGopStart: "
                    "gopset not set, syncing positionMap");
            SyncPositionMap();
            if (tempKeyFrameDist > 0 && !livetv)
            {
                VERBOSE(VB_PLAYBACK, LOC + "HandleGopStart: " +
                        QString("Initial key frame distance: %1.")
                        .arg(keyframedist));
                gopset       = true;
                reset_kfd    = true;
            }
        }
        else if (keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
        {
            VERBOSE(VB_PLAYBACK, LOC + "HandleGopStart: " +
                    QString("Key frame distance changed from %1 to %2.")
                    .arg(keyframedist).arg(tempKeyFrameDist));
            reset_kfd = true;
        }

        if (reset_kfd)
        {
            keyframedist    = tempKeyFrameDist;
            maxkeyframedist = max(keyframedist, maxkeyframedist);

            m_parent->SetKeyframeDistance(keyframedist);

#if 0
            // also reset length
            QMutexLocker locker(&m_positionMapLock);
            if (!m_positionMap.empty())
            {
                long long index       = m_positionMap.back().index;
                long long totframes   = index * keyframedist;
                uint length = (uint)((totframes * 1.0f) / fps);
                m_parent->SetFileLength(length, totframes);
            }
#endif
        }
    }

    lastKey = prevgoppos = framesRead - 1;

    if (can_reliably_parse_keyframes &&
        !hasFullPositionMap && !livetv && !watchingrecording)
    {
        long long last_frame = 0;
        {
            QMutexLocker locker(&m_positionMapLock);
            if (!m_positionMap.empty())
                last_frame = m_positionMap.back().index;
        }

        //cerr << "framesRead: " << framesRead << " last_frame: " << last_frame
        //    << " keyframedist: " << keyframedist << endl;

        // if we don't have an entry, fill it in with what we've just parsed
        if (framesRead > last_frame && keyframedist > 0)
        {
            long long startpos = pkt->pos;

            VERBOSE(VB_PLAYBACK+VB_TIMESTAMP, LOC +
                    QString("positionMap[ %1 ] == %2.")
                    .arg(framesRead).arg(startpos));

            PosMapEntry entry = {framesRead, framesRead, startpos};

            QMutexLocker locker(&m_positionMapLock);
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
            m_parent->SetFileLength((int)(secs), (int)(secs * fps));
        }
#endif
    }
}

#define SEQ_START     0x000001b3
#define GOP_START     0x000001b8
#define PICTURE_START 0x00000100
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af
#define SEQ_END_CODE  0x000001b7

void AvFormatDecoder::MpegPreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = stream->codec;
    const uint8_t *bufptr = pkt->data;
    const uint8_t *bufend = pkt->data + pkt->size;

    while (bufptr < bufend)
    {
        bufptr = ff_find_start_code(bufptr, bufend, &start_code_state);

        if (ringBuffer->isDVD() && (start_code_state == SEQ_END_CODE))
            ringBuffer->DVD()->NewSequence(true);

        if (start_code_state >= SLICE_MIN && start_code_state <= SLICE_MAX)
            continue;
        else if (SEQ_START == start_code_state)
        {
            if (bufptr + 11 >= pkt->data + pkt->size)
                continue; // not enough valid data...
            SequenceHeader *seq = reinterpret_cast<SequenceHeader*>(
                const_cast<uint8_t*>(bufptr));

            uint  width  = seq->width()  >> context->lowres;
            uint  height = seq->height() >> context->lowres;
            float aspect = seq->aspect(context->sub_id == 1);
            float seqFPS = seq->fps();

            bool changed = (seqFPS > fps+0.01f) || (seqFPS < fps-0.01f);
            changed |= (width  != (uint)current_width );
            changed |= (height != (uint)current_height);
            changed |= fabs(aspect - current_aspect) > eps;

            if (changed)
            {
                m_parent->SetVideoParams(width, height, seqFPS,
                                         keyframedist, aspect,
                                         kScan_Detect);

                current_width  = width;
                current_height = height;
                current_aspect = aspect;
                fps            = seqFPS;

                if (private_dec)
                    private_dec->Reset();

                gopset = false;
                prevgoppos = 0;
                lastapts = lastvpts = lastccptsu = 0;
                faulty_pts = faulty_dts = 0;
                last_pts_for_fault_detection = 0;
                last_dts_for_fault_detection = 0;
                pts_detected = false;
                reordered_pts_detected = false;

                // fps debugging info
                float avFPS = normalized_fps(stream, context);
                if ((seqFPS > avFPS+0.01f) || (seqFPS < avFPS-0.01f))
                {
                    VERBOSE(VB_PLAYBACK, LOC +
                            QString("avFPS(%1) != seqFPS(%2)")
                            .arg(avFPS).arg(seqFPS));
                }
            }

            seq_count++;

            if (!seen_gop && seq_count > 1)
            {
                HandleGopStart(pkt, true);
                pkt->flags |= PKT_FLAG_KEY;
            }
        }
        else if (GOP_START == start_code_state)
        {
            HandleGopStart(pkt, true);
            seen_gop = true;
            pkt->flags |= PKT_FLAG_KEY;
        }
    }
}

bool AvFormatDecoder::H264PreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = stream->codec;
    const uint8_t  *buf     = pkt->data;
    const uint8_t  *buf_end = pkt->data + pkt->size;
    bool on_frame = false;

    // crude NAL unit vs Annex B detection.
    // the parser only understands Annex B
    if (context->extradata && context->extradata_size >= 4)
    {
        int nal_size    = 0;
        int size_length = (context->extradata[4] & 0x3) + 1;

        for (int i = 0; i < size_length; i++)
            nal_size += buf[i];

        if (nal_size)
        {
            if (pkt->flags & PKT_FLAG_KEY)
                HandleGopStart(pkt, false);
            return true;
        }
    }

    while (buf < buf_end)
    {
        buf += m_h264_parser->addBytes(buf, buf_end - buf, 0);

        if (m_h264_parser->stateChanged())
        {
            if (m_h264_parser->FieldType() != H264Parser::FIELD_BOTTOM)
            {
                if (m_h264_parser->onFrameStart())
                    on_frame = true;

                if (!m_h264_parser->onKeyFrameStart())
                    continue;
            }
            else
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        float aspect_ratio = get_aspect(*context);
        QSize dim    = get_video_dim(*context);
        uint  width  = dim.width();
        uint  height = dim.height();
        float seqFPS = normalized_fps(stream, context);

        bool changed = (seqFPS > fps+0.01f) || (seqFPS < fps-0.01f);
        changed |= (width  != (uint)current_width );
        changed |= (height != (uint)current_height);
        changed |= fabs(aspect_ratio - current_aspect) > eps;

        if (changed)
        {
            m_parent->SetVideoParams(width, height, seqFPS,
                                     keyframedist, aspect_ratio,
                                     kScan_Detect);

            current_width  = width;
            current_height = height;
            current_aspect = aspect_ratio;
            fps            = seqFPS;

            gopset = false;
            prevgoppos = 0;
            lastapts = lastvpts = lastccptsu = 0;
            faulty_pts = faulty_dts = 0;
            last_pts_for_fault_detection = 0;
            last_dts_for_fault_detection = 0;
            pts_detected = false;
            reordered_pts_detected = false;

            // fps debugging info
            float avFPS = normalized_fps(stream, context);
            if ((seqFPS > avFPS+0.01f) || (seqFPS < avFPS-0.01f))
            {
                VERBOSE(VB_PLAYBACK, LOC +
                        QString("avFPS(%1) != seqFPS(%2)")
                        .arg(avFPS).arg(seqFPS));
            }
        }

        HandleGopStart(pkt, true);
        pkt->flags |= PKT_FLAG_KEY;
    }

    return on_frame;
}

bool AvFormatDecoder::PreProcessVideoPacket(AVStream *curstream, AVPacket *pkt)
{
    AVCodecContext *context = curstream->codec;
    bool on_frame = true;

    if (CODEC_IS_FFMPEG_MPEG(context->codec_id))
    {
        MpegPreProcessPkt(curstream, pkt);
    }
    else if (CODEC_IS_H264(context->codec_id))
    {
        on_frame = H264PreProcessPkt(curstream, pkt);
    }
    else
    {
        if (pkt->flags & PKT_FLAG_KEY)
        {
            HandleGopStart(pkt, false);
            seen_gop = true;
        }
        else
        {
            seq_count++;
            if (!seen_gop && seq_count > 1)
            {
                HandleGopStart(pkt, false);
            }
        }
    }

    if (framesRead == 0 && !justAfterChange &&
        !(pkt->flags & PKT_FLAG_KEY))
    {
        av_free_packet(pkt);
        return false;
    }

    if (on_frame)
        framesRead++;

    justAfterChange = false;

    if (exitafterdecoded)
        gotvideo = 1;

    return true;
}

bool AvFormatDecoder::ProcessVideoPacket(AVStream *curstream, AVPacket *pkt)
{
    int ret = 0, gotpicture = 0;
    int64_t pts = 0;
    AVCodecContext *context = curstream->codec;
    AVFrame mpa_pic;
    avcodec_get_frame_defaults(&mpa_pic);
    mpa_pic.reordered_opaque = AV_NOPTS_VALUE;

    if (pkt->pts != (int64_t)AV_NOPTS_VALUE)
        pts_detected = true;

    avcodeclock->lock();
    if (private_dec)
    {
        if (QString(ic->iformat->name).contains("avi") || !pts_detected)
            pkt->pts = pkt->dts;
        // TODO disallow private decoders for dvd playback
        // N.B. we do not reparse the frame as it breaks playback for
        // everything but libmpeg2
        ret = private_dec->GetFrame(curstream, &mpa_pic, &gotpicture, pkt);
    }
    else
    {
        context->reordered_opaque = pkt->pts;
        ret = avcodec_decode_video2(context, &mpa_pic, &gotpicture, pkt);
        // Reparse it to not drop the DVD still frame
        if (ringBuffer->isDVD() && ringBuffer->DVD()->NeedsStillFrame())
            ret = avcodec_decode_video2(context, &mpa_pic, &gotpicture, pkt);
    }
    avcodeclock->unlock();

    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unknown decoding error");
        return false;
    }

    if (!gotpicture)
    {
        return true;
    }

    // Decode CEA-608 and CEA-708 captions
    for (uint i = 0; i < (uint)mpa_pic.atsc_cc_len;
    i += ((mpa_pic.atsc_cc_buf[i] & 0x1f) * 3) + 2)
    {
        DecodeDTVCC(mpa_pic.atsc_cc_buf + i,
                    mpa_pic.atsc_cc_len - i);
    }

    VideoFrame *picframe = (VideoFrame *)(mpa_pic.opaque);

    if (!directrendering)
    {
        AVPicture tmppicture;

        VideoFrame *xf = picframe;
        picframe = m_parent->GetNextVideoFrame(false);

        unsigned char *buf = picframe->buf;
        avpicture_fill(&tmppicture, buf, PIX_FMT_YUV420P, context->width,
                       context->height);
        tmppicture.data[0] = buf + picframe->offsets[0];
        tmppicture.data[1] = buf + picframe->offsets[1];
        tmppicture.data[2] = buf + picframe->offsets[2];
        tmppicture.linesize[0] = picframe->pitches[0];
        tmppicture.linesize[1] = picframe->pitches[1];
        tmppicture.linesize[2] = picframe->pitches[2];

        QSize dim = get_video_dim(*context);
        sws_ctx = sws_getCachedContext(sws_ctx, context->width,
                                       context->height, context->pix_fmt,
                                       context->width, context->height,
                                       PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                       NULL, NULL, NULL);
        if (!sws_ctx)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate sws context");
            return false;
        }
        sws_scale(sws_ctx, mpa_pic.data, mpa_pic.linesize, 0, dim.height(),
                  tmppicture.data, tmppicture.linesize);


        if (xf)
        {
            // Set the frame flags, but then discard it
            // since we are not using it for display.
            xf->interlaced_frame = mpa_pic.interlaced_frame;
            xf->top_field_first = mpa_pic.top_field_first;
            xf->frameNumber = framesPlayed;
            m_parent->DiscardVideoFrame(xf);
        }
    }
    else if (!picframe)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "NULL videoframe - direct rendering not"
                "correctly initialized.");
        return false;
    }

    // Detect faulty video timestamps using logic from ffplay.
    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
    {
        faulty_dts += (pkt->dts <= last_dts_for_fault_detection);
        last_dts_for_fault_detection = pkt->dts;
    }
    if (mpa_pic.reordered_opaque != (int64_t)AV_NOPTS_VALUE)
    {
        faulty_pts += (mpa_pic.reordered_opaque <= last_pts_for_fault_detection);
        last_pts_for_fault_detection = mpa_pic.reordered_opaque;
        reordered_pts_detected = true;
    }

    // Explicity use DTS for DVD since they should always be valid for every
    // frame and fixups aren't enabled for DVD.
    // Select reordered_opaque (PTS) timestamps if they are less faulty or the
    // the DTS timestamp is missing. Also use fixups for missing PTS instead of
    // DTS to avoid oscillating between PTS and DTS. Only select DTS if PTS is
    // more faulty or never detected.
    if (ringBuffer->isDVD())
    {
        if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
            pts = pkt->dts;
        pts_selected = false;
    }
    else if (private_dec && private_dec->NeedsReorderedPTS() &&
             mpa_pic.reordered_opaque != (int64_t)AV_NOPTS_VALUE)
    {
        pts = mpa_pic.reordered_opaque;
        pts_selected = true;
    }
    else if (faulty_pts <= faulty_dts && reordered_pts_detected)
    {
        if (mpa_pic.reordered_opaque != (int64_t)AV_NOPTS_VALUE)
            pts = mpa_pic.reordered_opaque;
        pts_selected = true;
    }
    else if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
    {
        pts = pkt->dts;
        pts_selected = false;
    }
    long long vpts = (long long)(av_q2d(curstream->time_base) * pts * 1000);

    long long temppts = vpts;
    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!ringBuffer->isDVD() &&
        temppts <= lastvpts &&
        (temppts + (1000 / fps) > lastvpts || temppts <= 0))
    {
        temppts = lastvpts;
        temppts += (long long)(1000 / fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += (long long)(mpa_pic.repeat_pict * 500 / fps);
    }

    VERBOSE(VB_PLAYBACK+VB_TIMESTAMP, LOC +
            QString("video timecode %1 %2 %3 %4 %5 %6 (%7 active)%8")
                    .arg(mpa_pic.reordered_opaque).arg(pkt->pts).arg(pkt->dts)
                    .arg(vpts).arg(temppts).arg(lastvpts)
                    .arg((pts_selected) ? "reordered" : "dts")
                    .arg((vpts != temppts) ? " fixup" : ""));

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
    picframe->top_field_first  = mpa_pic.top_field_first;
    picframe->repeat_pict      = mpa_pic.repeat_pict;
    picframe->disp_timecode    = NormalizeVideoTimecode(curstream, temppts);
    picframe->frameNumber      = framesPlayed;

    m_parent->ReleaseNextVideoFrame(picframe, temppts);
    if (private_dec && mpa_pic.data[3])
        context->release_buffer(context, &mpa_pic);

    decoded_video_frame = picframe;
    gotvideo = 1;
    framesPlayed++;

    lastvpts = temppts;

    return true;
}

// TODO this is a temporary implementation. Remove code duplication with
// ProcessVideoPacket for 0.25
bool AvFormatDecoder::ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic)
{
    long long pts = 0;
    AVCodecContext *context = stream->codec;

    // Decode CEA-608 and CEA-708 captions
    for (uint i = 0; i < (uint)mpa_pic->atsc_cc_len;
    i += ((mpa_pic->atsc_cc_buf[i] & 0x1f) * 3) + 2)
    {
        DecodeDTVCC(mpa_pic->atsc_cc_buf + i,
                    mpa_pic->atsc_cc_len - i);
    }

    VideoFrame *picframe = (VideoFrame *)(mpa_pic->opaque);

    if (!directrendering)
    {
        AVPicture tmppicture;

        VideoFrame *xf = picframe;
        picframe = m_parent->GetNextVideoFrame(false);

        unsigned char *buf = picframe->buf;
        avpicture_fill(&tmppicture, buf, PIX_FMT_YUV420P, context->width,
                       context->height);
        tmppicture.data[0] = buf + picframe->offsets[0];
        tmppicture.data[1] = buf + picframe->offsets[1];
        tmppicture.data[2] = buf + picframe->offsets[2];
        tmppicture.linesize[0] = picframe->pitches[0];
        tmppicture.linesize[1] = picframe->pitches[1];
        tmppicture.linesize[2] = picframe->pitches[2];

        QSize dim = get_video_dim(*context);
        sws_ctx = sws_getCachedContext(sws_ctx, context->width,
                                       context->height, context->pix_fmt,
                                       context->width, context->height,
                                       PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                       NULL, NULL, NULL);
        if (!sws_ctx)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate sws context");
            return false;
        }
        sws_scale(sws_ctx, mpa_pic->data, mpa_pic->linesize, 0, dim.height(),
                  tmppicture.data, tmppicture.linesize);

        if (xf)
        {
            // Set the frame flags, but then discard it
            // since we are not using it for display.
            xf->interlaced_frame = mpa_pic->interlaced_frame;
            xf->top_field_first = mpa_pic->top_field_first;
            xf->frameNumber = framesPlayed;
            m_parent->DiscardVideoFrame(xf);
        }
    }
    else if (!picframe)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "NULL videoframe - direct rendering not"
                "correctly initialized.");
        return false;
    }

    pts = (long long)(av_q2d(stream->time_base) * mpa_pic->reordered_opaque * 1000);

    long long temppts = pts;

    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!ringBuffer->isDVD() &&
        temppts <= lastvpts &&
        (temppts + (1000 / fps) > lastvpts || temppts <= 0))
    {
        temppts = lastvpts;
        temppts += (long long)(1000 / fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += (long long)(mpa_pic->repeat_pict * 500 / fps);
    }

    VERBOSE(VB_PLAYBACK+VB_TIMESTAMP, LOC +
            QString("ProcessVideoFrame timecode %1 %2 %3%4")
            .arg(mpa_pic->reordered_opaque).arg(pts).arg(temppts).arg(lastvpts)
            .arg((pts != temppts) ? " fixup" : ""));

    picframe->interlaced_frame = mpa_pic->interlaced_frame;
    picframe->top_field_first = mpa_pic->top_field_first;
    picframe->repeat_pict = mpa_pic->repeat_pict;

    picframe->frameNumber = framesPlayed;
    m_parent->ReleaseNextVideoFrame(picframe, temppts);
    if (private_dec && mpa_pic->data[3])
        context->release_buffer(context, mpa_pic);

    decoded_video_frame = picframe;
    gotvideo = 1;
    framesPlayed++;

    lastvpts = temppts;

    return true;
}

/** \fn AvFormatDecoder::ProcessVBIDataPacket(const AVStream*, const AVPacket*)
 *  \brief Process ivtv proprietary embedded vertical blanking
 *         interval captions.
 *  \sa CC608Decoder, TeletextDecoder
 */
void AvFormatDecoder::ProcessVBIDataPacket(
    const AVStream *stream, const AVPacket *pkt)
{
    (void) stream;

    const uint8_t *buf     = pkt->data;
    uint64_t linemask      = 0;
    unsigned long long utc = lastccptsu;

    // [i]tv0 means there is a linemask
    // [I]TV0 means there is no linemask and all lines are present
    if ((buf[0]=='t') && (buf[1]=='v') && (buf[2] == '0'))
    {
        /// TODO this is almost certainly not endian safe....
        memcpy(&linemask, buf + 3, 8);
        buf += 11;
    }
    else if ((buf[0]=='T') && (buf[1]=='V') && (buf[2] == '0'))
    {
        linemask = 0xffffffffffffffffLL;
        buf += 3;
    }
    else
    {
        VERBOSE(VB_VBI, LOC + QString("Unknown VBI data stream '%1%2%3'")
                .arg(QChar(buf[0])).arg(QChar(buf[1])).arg(QChar(buf[2])));
        return;
    }

    static const uint min_blank = 6;
    for (uint i = 0; i < 36; i++)
    {
        if (!((linemask >> i) & 0x1))
            continue;

        const uint line  = ((i < 18) ? i : i-18) + min_blank;
        const uint field = (i<18) ? 0 : 1;
        const uint id2 = *buf & 0xf;
        switch (id2)
        {
            case VBI_TYPE_TELETEXT:
                // SECAM lines  6-23
                // PAL   lines  6-22
                // NTSC  lines 10-21 (rare)
                if (tracks[kTrackTypeTeletextMenu].empty())
                {
                    StreamInfo si(pkt->stream_index, 0, 0, 0, 0);
                    tracks[kTrackTypeTeletextMenu].push_back(si);
                }
                ttd->Decode(buf+1, VBI_IVTV);
                break;
            case VBI_TYPE_CC:
                // PAL   line 22 (rare)
                // NTSC  line 21
                if (21 == line)
                {
                    int data = (buf[2] << 8) | buf[1];
                    if (cc608_good_parity(cc608_parity_table, data))
                        ccd608->FormatCCField(utc/1000, field, data);
                    utc += 33367;
                }
                break;
            case VBI_TYPE_VPS: // Video Programming System
                // PAL   line 16
                ccd608->DecodeVPS(buf+1); // a.k.a. PDC
                break;
            case VBI_TYPE_WSS: // Wide Screen Signal
                // PAL   line 23
                // NTSC  line 20
                ccd608->DecodeWSS(buf+1);
                break;
        }
        buf += 43;
    }
    lastccptsu = utc;
    UpdateCaptionTracksFromStreams(true, false);
}

/** \fn AvFormatDecoder::ProcessDVBDataPacket(const AVStream*, const AVPacket*)
 *  \brief Process DVB Teletext.
 *  \sa TeletextDecoder
 */
void AvFormatDecoder::ProcessDVBDataPacket(
    const AVStream*, const AVPacket *pkt)
{
    const uint8_t *buf     = pkt->data;
    const uint8_t *buf_end = pkt->data + pkt->size;


    while (buf < buf_end)
    {
        if (*buf == 0x10)
        {
            buf++; // skip
        }
        else if (*buf == 0x02)
        {
            buf += 4;
            if ((buf_end - buf) >= 42)
                ttd->Decode(buf, VBI_DVB);
            buf += 42;
        }
        else if (*buf == 0x03)
        {
            buf += 4;
            if ((buf_end - buf) >= 42)
                ttd->Decode(buf, VBI_DVB_SUBTITLE);
            buf += 42;
        }
        else if (*buf == 0xff)
        {
            buf += 3;
        }
        else
        {
            VERBOSE(VB_VBI, QString("VBI: Unknown descriptor: %1").arg(*buf));
            buf += 46;
        }
    }
}

/** \fn AvFormatDecoder::ProcessDSMCCPacket(const AVStream*, const AVPacket*)
 *  \brief Process DSMCC object carousel packet.
 */
void AvFormatDecoder::ProcessDSMCCPacket(
    const AVStream *str, const AVPacket *pkt)
{
#ifdef USING_MHEG
    if (!itv && ! (itv = m_parent->GetInteractiveTV()))
        return;

    // The packet may contain several tables.
    uint8_t *data = pkt->data;
    int length = pkt->size;
    int componentTag, dataBroadcastId;
    unsigned carouselId;
    {
        QMutexLocker locker(avcodeclock);
        componentTag    = str->component_tag;
        dataBroadcastId = str->codec->flags;
        carouselId      = (unsigned) str->codec->sub_id;
    }
    while (length > 3)
    {
        uint16_t sectionLen = (((data[1] & 0xF) << 8) | data[2]) + 3;

        if (sectionLen > length) // This may well be filler
            return;

        itv->ProcessDSMCCSection(data, sectionLen,
                                 componentTag, carouselId,
                                 dataBroadcastId);
        length -= sectionLen;
        data += sectionLen;
    }
#endif // USING_MHEG
}

bool AvFormatDecoder::ProcessSubtitlePacket(AVStream *curstream, AVPacket *pkt)
{
    if (!subReader)
        return true;

    long long pts = 0;

    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    avcodeclock->lock();
    int subIdx = selectedTrack[kTrackTypeSubtitle].av_stream_index;
    avcodeclock->unlock();

    int gotSubtitles = 0;
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(AVSubtitle));

    if (ringBuffer->isDVD())
    {
        if (ringBuffer->DVD()->NumMenuButtons() > 0)
        {
            ringBuffer->DVD()->GetMenuSPUPkt(pkt->data, pkt->size,
                                             curstream->id);
        }
        else
        {
            if (pkt->stream_index == subIdx)
            {
                QMutexLocker locker(avcodeclock);
                ringBuffer->DVD()->DecodeSubtitles(&subtitle, &gotSubtitles,
                                                   pkt->data, pkt->size);
            }
        }
    }
    else if (pkt->stream_index == subIdx)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_decode_subtitle2(curstream->codec, &subtitle, &gotSubtitles,
                                 pkt);
    }

    if (gotSubtitles)
    {
        subtitle.start_display_time += pts;
        subtitle.end_display_time += pts;
        VERBOSE(VB_PLAYBACK|VB_TIMESTAMP, LOC +
                QString("subtl timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts)
                .arg(subtitle.start_display_time)
                .arg(subtitle.end_display_time));

        if (subReader)
        {
            subReader->AddAVSubtitle(subtitle,
                                 curstream->codec->codec_id == CODEC_ID_XSUB);
        }
    }

    return true;
}

bool AvFormatDecoder::ProcessRawTextPacket(AVPacket *pkt)
{
    if (!subReader ||
        selectedTrack[kTrackTypeRawText].av_stream_index != pkt->stream_index)
    {
        return false;
    }

    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    QTextDecoder *dec = codec->makeDecoder();
    QString text      = dec->toUnicode((const char*)pkt->data, pkt->size);
    QStringList list  = text.split('\n', QString::SkipEmptyParts);
    delete dec;
    subReader->AddRawTextSubtitle(list, pkt->convergence_duration);
    return true;
}

bool AvFormatDecoder::ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                        DecodeType decodetype)
{
    enum CodecID codec_id = curstream->codec->codec_id;

    switch (codec_id)
    {
    case CODEC_ID_MPEG2VBI:
        ProcessVBIDataPacket(curstream, pkt);
        break;
    case CODEC_ID_DVB_VBI:
        ProcessDVBDataPacket(curstream, pkt);
        break;
#ifdef USING_MHEG
    case CODEC_ID_DSMCC_B:
    {
        ProcessDSMCCPacket(curstream, pkt);

        // Have to return regularly to ensure that the OSD is updated.
        // This applies both to MHEG and also channel browsing.
        if (!(decodetype & kDecodeVideo))
            allowedquit |= (itv && itv->ImageHasChanged());
        break;
    }
#endif // USING_MHEG:
    }
    return true;
}

int AvFormatDecoder::SetTrack(uint type, int trackNo)
{
    bool ret = DecoderBase::SetTrack(type, trackNo);

    if (kTrackTypeAudio == type)
    {
        QString msg = SetupAudioStream() ? "" : "not ";
        VERBOSE(VB_AUDIO, LOC + "Audio stream type "+msg+"changed.");
    }

    return ret;
}

QString AvFormatDecoder::GetTrackDesc(uint type, uint trackNo) const
{
    if (trackNo >= tracks[type].size())
        return "";

    int lang_key = tracks[type][trackNo].language;
    if (kTrackTypeAudio == type)
    {
        if (ringBuffer->isDVD())
            lang_key = ringBuffer->DVD()->GetAudioLanguage(trackNo);

        QString msg = iso639_key_toName(lang_key);

        int av_index = tracks[kTrackTypeAudio][trackNo].av_stream_index;
        AVStream *s = ic->streams[av_index];

        if (!s)
            return QString("%1: %2").arg(trackNo + 1).arg(msg);

        if (s->codec->codec_id == CODEC_ID_MP3)
            msg += QString(" MP%1").arg(s->codec->sub_id);
        else if (s->codec->codec)
            msg += QString(" %1").arg(s->codec->codec->name).toUpper();

        int channels = 0;
        if (ringBuffer->isDVD())
            channels = ringBuffer->DVD()->GetNumAudioChannels(trackNo);
        else if (s->codec->channels)
            channels = tracks[kTrackTypeAudio][trackNo].orig_num_channels;

        if (channels == 0)
            msg += QString(" ?ch");
        else if((channels > 4) && !(channels & 1))
            msg += QString(" %1.1ch").arg(channels - 1);
        else
            msg += QString(" %1ch").arg(channels);

        return QString("%1: %2").arg(trackNo + 1).arg(msg);
    }
    else if (kTrackTypeSubtitle == type)
    {
        if (ringBuffer->isDVD())
            lang_key = ringBuffer->DVD()->GetSubtitleLanguage(trackNo);

        return QObject::tr("Subtitle") + QString(" %1: %2")
            .arg(trackNo + 1).arg(iso639_key_toName(lang_key));
    }
    else
    {
        return DecoderBase::GetTrackDesc(type, trackNo);
    }
}

int AvFormatDecoder::GetTeletextDecoderType(void) const
{
    return ttd->GetDecoderType();
}

QString AvFormatDecoder::GetXDS(const QString &key) const
{
    return ccd608->GetXDS(key);
}

bool AvFormatDecoder::SetAudioByComponentTag(int tag)
{
    for (uint i = 0; i < tracks[kTrackTypeAudio].size(); i++)
    {
        AVStream *s  = ic->streams[tracks[kTrackTypeAudio][i].av_stream_index];
        if (s)
        {
            if ((s->component_tag == tag) ||
                ((tag <= 0) && s->component_tag <= 0))
            {
                return SetTrack(kTrackTypeAudio, i);
            }
        }
    }
    return false;
}

bool AvFormatDecoder::SetVideoByComponentTag(int tag)
{
    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVStream *s  = ic->streams[i];
        if (s)
        {
            if (s->component_tag == tag)
            {
                StreamInfo si(i, 0, 0, 0, 0);
                selectedTrack[kTrackTypeVideo] = si;
                return true;
            }
        }
    }
    return false;
}

// documented in decoderbase.cpp
int AvFormatDecoder::AutoSelectTrack(uint type)
{
    if (kTrackTypeAudio == type)
        return AutoSelectAudioTrack();

    if (ringBuffer->InDVDMenuOrStillFrame())
        return -1;

    return DecoderBase::AutoSelectTrack(type);
}

static vector<int> filter_lang(const sinfo_vec_t &tracks, int lang_key)
{
    vector<int> ret;

    for (uint i = 0; i < tracks.size(); i++)
        if ((lang_key < 0) || tracks[i].language == lang_key)
            ret.push_back(i);

    return ret;
}

static int filter_max_ch(const AVFormatContext *ic,
                         const sinfo_vec_t     &tracks,
                         const vector<int>     &fs,
                         enum CodecID           codecId = CODEC_ID_NONE)
{
    int selectedTrack = -1, max_seen = -1;

    vector<int>::const_iterator it = fs.begin();
    for (; it != fs.end(); ++it)
    {
        const int stream_index = tracks[*it].av_stream_index;
        const AVCodecContext *ctx = ic->streams[stream_index]->codec;
        if ((codecId == CODEC_ID_NONE || codecId == ctx->codec_id) &&
            (max_seen < ctx->channels))
        {
            selectedTrack = *it;
            max_seen = ctx->channels;
        }
    }

    return selectedTrack;
}

/** \fn AvFormatDecoder::AutoSelectAudioTrack(void)
 *  \brief Selects the best audio track.
 *
 *   It is primarily needed for DVB recordings
 *
 *   This function will select the best audio track available
 *   using the following criteria, in order of decreasing
 *   preference:
 *
 *   1) The stream last selected by the user, which is
 *      recalled as the Nth stream in the preferred language
 *      or the Nth substream when audio is in dual language
 *      format (each channel contains a different language track)
 *      If it cannot be located we attempt to find a stream
 *      in the same language.
 *
 *   2) If we cannot reselect the last user selected stream,
 *      then for each preferred language from most preferred
 *      to least preferred, we try to find a new stream based
 *      on the algorithm below.
 *
 *   3) If we cannot select a stream in a preferred language
 *      we try to select a stream irrespective of language
 *      based on the algorithm below.
 *
 *   When searching for a new stream (ie. options 2 and 3
 *   above), the following search is carried out in order:
 *
 *   i)   If DTS passthrough is enabled then the DTS track with
 *        the greatest number of audio channels is selected
 *        (the first will be chosen if there are several the
 *        same). If DTS passthrough is not enabled this step
 *        will be skipped because internal DTS decoding is not
 *        currently supported.
 *
 *   ii)  If no DTS track is chosen, the AC3 track with the
 *        greatest number of audio channels is selected (the
 *        first will be chosen if there are several the same).
 *        Internal decoding of AC3 is supported, so this will
 *        be used irrespective of whether AC3 passthrough is
 *        enabled.
 *
 *   iii) Lastly the track with the greatest number of audio
 *        channels irrespective of type will be selected.
 *  \return track if a track was selected, -1 otherwise
 */
int AvFormatDecoder::AutoSelectAudioTrack(void)
{
    const sinfo_vec_t &atracks = tracks[kTrackTypeAudio];
    StreamInfo        &wtrack  = wantedTrack[kTrackTypeAudio];
    StreamInfo        &strack  = selectedTrack[kTrackTypeAudio];
    int               &ctrack  = currentTrack[kTrackTypeAudio];

    uint numStreams = atracks.size();
    if ((ctrack >= 0) && (ctrack < (int)numStreams))
        return ctrack; // audio already selected

#if 0
    // enable this to print streams
    for (uint i = 0; i < atracks.size(); i++)
    {
        int idx = atracks[i].av_stream_index;
        AVCodecContext *codec_ctx = ic->streams[idx]->codec;
        AudioInfo item(codec_ctx->codec_id, codec_ctx->bps,
                       codec_ctx->sample_rate, codec_ctx->channels,
                       DoPassThrough(codec_ctx));
        VERBOSE(VB_AUDIO, LOC + " * " + item.toString());
    }
#endif

    int selTrack = (1 == numStreams) ? 0 : -1;
    int wlang    = wtrack.language;

    if ((selTrack < 0) && (wtrack.av_substream_index >= 0))
    {
        VERBOSE(VB_AUDIO, LOC + "Trying to reselect audio sub-stream");
        // Dual stream without language information: choose
        // the previous substream that was kept in wtrack,
        // ignoring the stream index (which might have changed).
        int substream_index = wtrack.av_substream_index;

        for (uint i = 0; i < numStreams; i++)
        {
            if (atracks[i].av_substream_index == substream_index)
            {
                selTrack = i;
                break;
            }
        }
    }

    if ((selTrack < 0) && wlang >= -1 && numStreams)
    {
        VERBOSE(VB_AUDIO, LOC + "Trying to reselect audio track");
        // Try to reselect user selected subtitle stream.
        // This should find the stream after a commercial
        // break and in some cases after a channel change.
        uint windx = wtrack.language_index;
        for (uint i = 0; i < numStreams; i++)
        {
            if (wlang == atracks[i].language)
            {
                selTrack = i;

                if (windx == atracks[i].language_index)
                    break;
            }
        }
    }

    if (selTrack < 0 && numStreams)
    {
        VERBOSE(VB_AUDIO, LOC + "Trying to select audio track (w/lang)");

        // try to get the language track matching the frontend language.
        QString language_key_convert = iso639_str2_to_str3(gCoreContext->GetLanguage());
        uint language_key = iso639_str3_to_key(language_key_convert);
        uint canonical_key = iso639_key_to_canonical_key(language_key);

        vector<int> flang = filter_lang(atracks, canonical_key);

        selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_TRUEHD);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_EAC3);

        if (!transcoding && selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_DTS);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_AC3);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang);

        // try to get best track for most preferred language
        // Set by the "Guide Data" language prefs in Appearance.
        if (selTrack < 0)
        {
            vector<int>::const_iterator it = languagePreference.begin();
            for (; it !=  languagePreference.end() && selTrack < 0; ++it)
            {
                vector<int> flang = filter_lang(atracks, *it);

                selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_TRUEHD);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_EAC3);

                if (!transcoding && selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_DTS);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_AC3);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang);
            }
        }
        // try to get best track for any language
        if (selTrack < 0)
        {
            VERBOSE(VB_AUDIO, LOC + "Trying to select audio track (wo/lang)");
            vector<int> flang = filter_lang(atracks, -1);

            selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_TRUEHD);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_EAC3);

            if (!transcoding && selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_DTS);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, CODEC_ID_AC3);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang);
        }
    }

    if (selTrack < 0)
    {
        strack.av_stream_index = -1;
        if (ctrack != selTrack)
        {
            VERBOSE(VB_AUDIO, LOC + "No suitable audio track exists.");
            ctrack = selTrack;
        }
    }
    else
    {
        ctrack = selTrack;
        strack = atracks[selTrack];

        if (wtrack.av_stream_index < 0)
            wtrack = strack;

        VERBOSE(VB_AUDIO, LOC +
                QString("Selected track %1 (A/V Stream #%2)")
                .arg(GetTrackDesc(kTrackTypeAudio, ctrack))
                .arg(strack.av_stream_index));
    }

    SetupAudioStream();
    return selTrack;
}

static void extract_mono_channel(uint channel, AudioInfo *audioInfo,
                                 char *buffer, int bufsize)
{
    // Only stereo -> mono (left or right) is supported
    if (audioInfo->channels != 2)
        return;

    if (channel >= (uint)audioInfo->channels)
        return;

    const uint samplesize = audioInfo->sample_size;
    const uint samples    = bufsize / samplesize;
    const uint halfsample = samplesize >> 1;

    const char *from = (channel == 1) ? buffer + halfsample : buffer;
    char *to         = (channel == 0) ? buffer + halfsample : buffer;

    for (uint sample = 0; sample < samples;
         (sample++), (from += samplesize), (to += samplesize))
    {
        memmove(to, from, halfsample);
    }
}

bool AvFormatDecoder::ProcessAudioPacket(AVStream *curstream, AVPacket *pkt,
                                         DecodeType decodetype)
{
    AVCodecContext *ctx = curstream->codec;
    long long pts       = 0;
    int ret             = 0;
    int data_size       = 0;
    bool firstloop      = true, dts = false;

    avcodeclock->lock();
    int audIdx = selectedTrack[kTrackTypeAudio].av_stream_index;
    int audSubIdx = selectedTrack[kTrackTypeAudio].av_substream_index;
    avcodeclock->unlock();

    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    AVPacket tmp_pkt;
    tmp_pkt.data = pkt->data;
    tmp_pkt.size = pkt->size;

    while (tmp_pkt.size > 0)
    {
        bool reselectAudioTrack = false;

        /// HACK HACK HACK -- begin See #3731
        if (!m_audio->HasAudioIn())
        {
            VERBOSE(VB_AUDIO, LOC + "Audio is disabled - trying to restart it");
            reselectAudioTrack = true;
        }
        /// HACK HACK HACK -- end

        // detect switches between stereo and dual languages
        bool wasDual = audSubIdx != -1;
        bool isDual = ctx->avcodec_dual_language;
        if ((wasDual && !isDual) || (!wasDual &&  isDual))
        {
            SetupAudioStreamSubIndexes(audIdx);
            reselectAudioTrack = true;
        }

        // detect channels on streams that need
        // to be decoded before we can know this
        bool already_decoded = false;
        if (!ctx->channels)
        {
            VERBOSE(VB_IMPORTANT, LOC + QString("Setting channels to %1")
                    .arg(audioOut.channels));

            QMutexLocker locker(avcodeclock);

            if (DoPassThrough(ctx) || !DecoderWillDownmix(ctx))
            {
                // for passthru or codecs for which the decoder won't downmix
                // let the decoder set the number of channels. For other codecs
                // we downmix if necessary in audiooutputbase
                ctx->request_channels = 0;
            }
            else // No passthru, the decoder will downmix
            {
                ctx->request_channels = m_audio->GetMaxChannels();
                if (ctx->codec_id == CODEC_ID_AC3)
                    ctx->channels = m_audio->GetMaxChannels();
            }

            data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            ret = avcodec_decode_audio3(ctx, audioSamples,
                                        &data_size, &tmp_pkt);
            already_decoded = true;
            reselectAudioTrack |= ctx->channels;
        }

        if (reselectAudioTrack)
        {
            QMutexLocker locker(avcodeclock);
            currentTrack[kTrackTypeAudio] = -1;
            selectedTrack[kTrackTypeAudio].av_stream_index = -1;
            audIdx = -1;
            audSubIdx = -1;
            AutoSelectAudioTrack();
            audIdx = selectedTrack[kTrackTypeAudio].av_stream_index;
            audSubIdx = selectedTrack[kTrackTypeAudio].av_substream_index;
        }

        if (!(decodetype & kDecodeAudio) || (pkt->stream_index != audIdx))
            break;

        if (firstloop && pkt->pts != (int64_t)AV_NOPTS_VALUE)
            lastapts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);

        if (skipaudio && selectedTrack[kTrackTypeVideo].av_stream_index > -1)
        {
            if ((lastapts < lastvpts - (10.0 / fps)) || lastvpts == 0)
                break;
            else
                skipaudio = false;
        }

        avcodeclock->lock();
        data_size = 0;

        if (audioOut.do_passthru)
        {
            data_size = tmp_pkt.size;
            dts = CODEC_ID_DTS == ctx->codec_id;
            ret = encode_frame(dts, tmp_pkt.data, tmp_pkt.size, audioSamples,
                               data_size);
        }
        else
        {
            if (!already_decoded)
            {
                if (DecoderWillDownmix(ctx))
                {
                    ctx->request_channels = m_audio->GetMaxChannels();
                    if (ctx->codec_id == CODEC_ID_AC3)
                        ctx->channels = m_audio->GetMaxChannels();
                }
                else
                    ctx->request_channels = 0;

                data_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
                ret = avcodec_decode_audio3(ctx, audioSamples, &data_size,
                                            &tmp_pkt);
            }

            // When decoding some audio streams the number of
            // channels, etc isn't known until we try decoding it.
            if (ctx->sample_rate != audioOut.sample_rate ||
                ctx->channels    != audioOut.channels)
            {
                VERBOSE(VB_IMPORTANT, LOC + "Audio stream changed");
                currentTrack[kTrackTypeAudio] = -1;
                selectedTrack[kTrackTypeAudio].av_stream_index = -1;
                audIdx = -1;
                AutoSelectAudioTrack();
                data_size = 0;
            }
        }
        avcodeclock->unlock();

        if (ret < 0)
        {
            if (!dts)
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Unknown audio decoding error");
            return false;
        }

        if (data_size <= 0)
        {
            tmp_pkt.data += ret;
            tmp_pkt.size -= ret;
            continue;
        }

        long long temppts = lastapts;

        // calc for next frame
        lastapts += (long long)
            ((double)(data_size * 1000) /
             (ctx->sample_rate * (audioOut.do_passthru ?  2 : ctx->channels) *
              av_get_bits_per_sample_format(ctx->sample_fmt)>>3));

        VERBOSE(VB_PLAYBACK+VB_TIMESTAMP,
                LOC + QString("audio timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(lastapts));

        if (audSubIdx != -1)
            extract_mono_channel(audSubIdx, &audioOut,
                                 (char *)audioSamples, data_size);

        m_audio->AddAudioData((char *)audioSamples, data_size, temppts);

        allowedquit |=
            ringBuffer->InDVDMenuOrStillFrame() ||
            m_audio->IsBufferAlmostFull();

        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
        firstloop = false;
    }

    return true;
}

// documented in decoderbase.h
bool AvFormatDecoder::GetFrame(DecodeType decodetype)
{
    AVPacket *pkt = NULL;
    bool have_err = false;

    gotvideo = false;

    frame_decoded = 0;
    decoded_video_frame = NULL;

    allowedquit = false;
    bool storevideoframes = false;

    avcodeclock->lock();
    AutoSelectTracks();
    avcodeclock->unlock();

    skipaudio = (lastvpts == 0);

    bool has_video = HasVideo(ic);

    if (!has_video && (decodetype & kDecodeVideo))
    {
        gotvideo = GenerateDummyVideoFrame();
        decodetype = (DecodeType)((int)decodetype & ~kDecodeVideo);
        skipaudio = false;
    }

    allowedquit = m_audio->IsBufferAlmostFull();

    if (private_dec && private_dec->HasBufferedFrames() &&
       (selectedTrack[kTrackTypeVideo].av_stream_index > -1))
    {
        AVStream *stream = ic->streams[selectedTrack[kTrackTypeVideo]
                                 .av_stream_index];
        AVFrame mpa_pic;
        avcodec_get_frame_defaults(&mpa_pic);
        int got_picture = 0;
        private_dec->GetFrame(stream, &mpa_pic, &got_picture, NULL);
        if (got_picture)
            ProcessVideoFrame(stream, &mpa_pic);
    }

    while (!allowedquit)
    {
        if ((decodetype & kDecodeAudio) &&
            ((currentTrack[kTrackTypeAudio] < 0) ||
             (selectedTrack[kTrackTypeAudio].av_stream_index < 0)))
        {
            // disable audio request if there are no audio streams anymore
            // and we have video, otherwise allow decoding to stop
            if (has_video)
                decodetype = (DecodeType)((int)decodetype & ~kDecodeAudio);
            else
                allowedquit = true;
        }

        if (ringBuffer->isDVD())
        {
            // Update the title length
            if (m_parent->AtNormalSpeed() && ringBuffer->DVD()->PGCLengthChanged())
            {
                ResetPosMap();
                SyncPositionMap();
                UpdateFramesPlayed();
            }

            // rescan the non-video streams as necessary
            if (ringBuffer->DVD()->AudioStreamsChanged())
                ScanStreams(true);

            // always use the first video stream (must come after ScanStreams above)
            selectedTrack[kTrackTypeVideo].av_stream_index = 0;
        }

        if (gotvideo)
        {
            if (decodetype == kDecodeNothing)
            {
                // no need to buffer audio or video if we
                // only care about building a keyframe map.
                allowedquit = true;
                continue;
            }
            else if (lowbuffers && ((decodetype & kDecodeAV) == kDecodeAV) &&
                     storedPackets.count() < max_video_queue_size &&
                     lastapts < lastvpts + 100 &&
                     !ringBuffer->InDVDMenuOrStillFrame())
            {
                storevideoframes = true;
            }
            else if (decodetype & kDecodeVideo)
            {
                if (storedPackets.count() >= max_video_queue_size)
                    VERBOSE(VB_IMPORTANT, LOC_WARN +
                            QString("Audio %1 ms behind video but already %2 "
                               "video frames queued. AV-Sync might be broken.")
                            .arg(lastvpts-lastapts).arg(storedPackets.count()));
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
            pkt = storedPackets.takeFirst();
        }
        else
        {
            if (!pkt)
            {
                pkt = new AVPacket;
                bzero(pkt, sizeof(AVPacket));
                av_init_packet(pkt);
            }

            int retval;
            if (!ic || ((retval = av_read_frame(ic, pkt)) < 0))
            {
                if (retval == -EAGAIN)
                    continue;

                SetEof(true);
                delete pkt;
                return false;
            }

            if (waitingForChange && pkt->pos >= readAdjust)
                FileChanged();

            if (pkt->pos > readAdjust)
                pkt->pos -= readAdjust;
        }

        if (pkt->stream_index >= (int)ic->nb_streams)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Bad stream");
            av_free_packet(pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt->stream_index];

        if (!curstream)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Bad stream (NULL)");
            av_free_packet(pkt);
            continue;
        }

        if (ringBuffer->isDVD() &&
            curstream->codec->codec_type == CODEC_TYPE_VIDEO)
        {
#ifdef USING_XVMC
            if (!private_dec)
            {
                int current_width = curstream->codec->width;
                int video_width = m_parent->GetVideoSize().width();
                if (dvd_xvmc_enabled && m_parent && m_parent->getVideoOutput())
                {
                    bool dvd_xvmc_active = false;
                    if (codec_is_xvmc(video_codec_id))
                    {
                        dvd_xvmc_active = true;
                    }

                    bool indvdmenu   = ringBuffer->InDVDMenuOrStillFrame();
                    if ((indvdmenu && dvd_xvmc_active) ||
                        ((!indvdmenu && !dvd_xvmc_active)))
                    {
                        VERBOSE(VB_PLAYBACK, LOC + QString("DVD Codec Change "
                                    "indvdmenu %1 dvd_xvmc_active %2")
                                .arg(indvdmenu).arg(dvd_xvmc_active));
                        dvd_video_codec_changed = true;
                    }
                }

                if ((video_width > 0) && dvd_video_codec_changed)
                {
                    VERBOSE(VB_PLAYBACK, LOC + QString("DVD Stream/Codec Change "
                                "video_width %1 current_width %2 "
                                "dvd_video_codec_changed %3")
                            .arg(video_width).arg(current_width)
                            .arg(dvd_video_codec_changed));
                    av_free_packet(pkt);
                    if (current_width > 0) {
                        CloseCodecs();
                        ScanStreams(false);
                        allowedquit = true;
                        dvd_video_codec_changed = false;
                    }
                    continue;
                }
            }
#endif //USING_XVMC
        }

        enum CodecType codec_type = curstream->codec->codec_type;

        if (storevideoframes && codec_type == CODEC_TYPE_VIDEO)
        {
            av_dup_packet(pkt);
            storedPackets.append(pkt);
            pkt = NULL;
            continue;
        }

        if (codec_type == CODEC_TYPE_VIDEO &&
            pkt->stream_index == selectedTrack[kTrackTypeVideo].av_stream_index)
        {
            if (!PreProcessVideoPacket(curstream, pkt))
                continue;

            // If the resolution changed in XXXPreProcessPkt, we may
            // have a fatal error, so check for this before continuing.
            if (m_parent->IsErrored())
            {
                av_free_packet(pkt);
                delete pkt;
                return false;
            }
        }

        if (codec_type == CODEC_TYPE_SUBTITLE &&
            curstream->codec->codec_id == CODEC_ID_TEXT)
        {
            ProcessRawTextPacket(pkt);
            av_free_packet(pkt);
            continue;
        }

        if (codec_type == CODEC_TYPE_SUBTITLE &&
            curstream->codec->codec_id == CODEC_ID_DVB_TELETEXT)
        {
            ProcessDVBDataPacket(curstream, pkt);
            av_free_packet(pkt);
            continue;
        }

        if (codec_type == CODEC_TYPE_DATA)
        {
            ProcessDataPacket(curstream, pkt, decodetype);
            av_free_packet(pkt);
            continue;
        }

        if (!curstream->codec->codec)
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("No codec for stream index %1, type(%2) id(%3:%4)")
                    .arg(pkt->stream_index)
                    .arg(ff_codec_type_string(codec_type))
                    .arg(ff_codec_id_string(curstream->codec->codec_id))
                    .arg(curstream->codec->codec_id));
            av_free_packet(pkt);
            continue;
        }

        have_err = false;

        switch (codec_type)
        {
            case CODEC_TYPE_AUDIO:
            {
                if (!ProcessAudioPacket(curstream, pkt, decodetype))
                    have_err = true;
                break;
            }

            case CODEC_TYPE_VIDEO:
            {
                if (pkt->stream_index != selectedTrack[kTrackTypeVideo].av_stream_index)
                {
                    break;
                }

                if (pkt->pts != (int64_t) AV_NOPTS_VALUE)
                {
                    lastccptsu = (long long)
                                 (av_q2d(curstream->time_base)*pkt->pts*1000000);
                }

                if (!(decodetype & kDecodeVideo))
                {
                    framesPlayed++;
                    gotvideo = 1;
                    break;
                }

                if (!ProcessVideoPacket(curstream, pkt))
                    have_err = true;
                break;
            }

            case CODEC_TYPE_SUBTITLE:
            {
                if (!ProcessSubtitlePacket(curstream, pkt))
                    have_err = true;
                break;
            }

            default:
            {
                AVCodecContext *enc = curstream->codec;
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("Decoding - id(%1) type(%2)")
                        .arg(ff_codec_id_string(enc->codec_id))
                        .arg(ff_codec_type_string(enc->codec_type)));
                have_err = true;
                break;
            }
        }

        if (!have_err)
            frame_decoded = 1;

        av_free_packet(pkt);
    }

    if (pkt)
        delete pkt;

    return true;
}

bool AvFormatDecoder::HasVideo(const AVFormatContext *ic)
{
    if (!ic || !ic->cur_pmt_sect)
        return true;

    const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
    const PSIPTable psip(pes);
    const ProgramMapTable pmt(psip);

    bool has_video = false;
    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        // MythTV remaps OpenCable Video to normal video during recording
        // so "dvb" is the safest choice for system info type, since this
        // will ignore other uses of the same stream id in DVB countries.
        has_video |= pmt.IsVideo(i, "dvb");

        // MHEG may explicitly select a private stream as video
        has_video |= ((i == (uint)selectedTrack[kTrackTypeVideo].av_stream_index) &&
                      (pmt.StreamType(i) == StreamID::PrivData));
    }

    return has_video;
}

bool AvFormatDecoder::GenerateDummyVideoFrame(void)
{
    if (!m_parent->getVideoOutput())
        return false;

    VideoFrame *frame = m_parent->GetNextVideoFrame(true);
    if (!frame)
        return false;

    if (dummy_frame && !compatible(frame, dummy_frame))
    {
        delete [] dummy_frame->buf;
        delete dummy_frame;
        dummy_frame = NULL;
    }

    if (!dummy_frame)
    {
        dummy_frame = new VideoFrame;
        init(dummy_frame,
             frame->codec, new unsigned char[frame->size],
             frame->width, frame->height, frame->size,
             frame->pitches, frame->offsets);

        clear(dummy_frame);
        // Note: instead of clearing the frame to black, one
        // could load an image or a series of images...

        dummy_frame->interlaced_frame = 0; // not interlaced
        dummy_frame->top_field_first  = 1; // top field first
        dummy_frame->repeat_pict      = 0; // not a repeated picture
    }

    copy(frame, dummy_frame);

    frame->frameNumber = framesPlayed;

    m_parent->ReleaseNextVideoFrame(frame, lastvpts);
    m_parent->getVideoOutput()->DeLimboFrame(frame);

    decoded_video_frame = frame;
    framesPlayed++;

    return true;
}

QString AvFormatDecoder::GetCodecDecoderName(void) const
{
    if (private_dec)
        return private_dec->GetName();
    return get_decoder_name(video_codec_id);
}

QString AvFormatDecoder::GetRawEncodingType(void)
{
    int stream = selectedTrack[kTrackTypeVideo].av_stream_index;
    if (stream < 0 || !ic)
        return QString();
    return ff_codec_id_string(ic->streams[stream]->codec->codec_id);
}

void *AvFormatDecoder::GetVideoCodecPrivate(void)
{
    return NULL; // TODO is this still needed
}

void AvFormatDecoder::SetDisablePassThrough(bool disable)
{
    // can only disable never re-enable as once
    // timestretch is on its on for the session
    if (disable_passthru)
        return;

    if (selectedTrack[kTrackTypeAudio].av_stream_index < 0)
    {
        disable_passthru = disable;
        return;
    }

    if (disable != disable_passthru)
    {
        disable_passthru = disable;
        QString msg = (disable) ? "Disabling" : "Allowing";
        VERBOSE(VB_AUDIO, LOC + msg + " pass through");

        // Force pass through state to be reanalyzed
        QMutexLocker locker(avcodeclock);
        SetupAudioStream();
    }
}

inline bool AvFormatDecoder::DecoderWillDownmix(const AVCodecContext *ctx)
{
    // Until ffmpeg properly implements dialnorm
    // use Myth internal downmixer if machines has FPU/SSE
    if (!m_audio->CanDownmix() || !AudioOutputUtil::has_hardware_fpu())
        return true;

    switch (ctx->codec_id)
    {
        case CODEC_ID_AC3:
        case CODEC_ID_TRUEHD:
        case CODEC_ID_EAC3:
            return true;
        default:
            return false;
    }
}

bool AvFormatDecoder::DoPassThrough(const AVCodecContext *ctx)
{
    bool passthru = false;

    if (ctx->codec_id == CODEC_ID_AC3)
        passthru = m_audio->CanAC3();
    else if (ctx->codec_id == CODEC_ID_DTS)
        passthru = m_audio->CanDTS();
    passthru &= m_audio->CanPassthrough(ctx->sample_rate, ctx->channels);
    passthru &= !internal_vol;
    passthru &= !transcoding && !disable_passthru;

    return passthru;
}

/** \fn AvFormatDecoder::SetupAudioStream(void)
 *  \brief Reinitializes audio if it needs to be reinitialized.
 *
 *   NOTE: The avcodeclock must be held when this is called.
 *
 *  \return true if audio changed, false otherwise
 */
bool AvFormatDecoder::SetupAudioStream(void)
{
    AudioInfo info; // no_audio
    AVStream *curstream = NULL;
    AVCodecContext *ctx = NULL;
    AudioInfo old_in    = audioIn;
    bool using_passthru = false;
    int  orig_channels  = 2;

    if ((currentTrack[kTrackTypeAudio] >= 0) && ic &&
        (selectedTrack[kTrackTypeAudio].av_stream_index <=
         (int) ic->nb_streams) &&
        (curstream = ic->streams[selectedTrack[kTrackTypeAudio]
                                 .av_stream_index]))
    {
        assert(curstream);
        assert(curstream->codec);
        ctx = curstream->codec;
        orig_channels = selectedTrack[kTrackTypeAudio].orig_num_channels;
        AudioFormat fmt;

        switch (ctx->sample_fmt)
        {
            case SAMPLE_FMT_U8:     fmt = FORMAT_U8;    break;
            case SAMPLE_FMT_S16:    fmt = FORMAT_S16;   break;
            case SAMPLE_FMT_FLT:    fmt = FORMAT_FLT;   break;
            case SAMPLE_FMT_DBL:    fmt = FORMAT_NONE;  break;
            case SAMPLE_FMT_S32:
                switch (ctx->bits_per_raw_sample)
                {
                    case  0:    fmt = FORMAT_S32;   break;
                    case 24:    fmt = FORMAT_S24;   break;
                    case 32:    fmt = FORMAT_S32;   break;
                    default:    fmt = FORMAT_NONE;
                }
                break;
            default:                fmt = FORMAT_NONE;
        }

        if (fmt == FORMAT_NONE)
        {
            int bps = av_get_bits_per_sample_format(ctx->sample_fmt);
            if (ctx->sample_fmt == SAMPLE_FMT_S32 && ctx->bits_per_raw_sample)
                bps = ctx->bits_per_raw_sample;
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Unsupported sample format "
                                                    "with %1 bits").arg(bps));
            return false;
        }

        using_passthru = DoPassThrough(ctx);

        ctx->request_channels = ctx->channels;

        if (!using_passthru &&
            ctx->channels > (int)m_audio->GetMaxChannels() &&
            DecoderWillDownmix(ctx))
        {
            ctx->request_channels = m_audio->GetMaxChannels();
        }

        info = AudioInfo(ctx->codec_id, fmt, ctx->sample_rate,
                         ctx->channels, using_passthru, orig_channels);
    }

    if (!ctx)
    {
        VERBOSE(VB_PLAYBACK, LOC + "No codec context. Returning false");
        return false;
    }

    if (info == audioIn)
        return false; // no change

    VERBOSE(VB_AUDIO, LOC + "Initializing audio parms from " +
            QString("audio track #%1").arg(currentTrack[kTrackTypeAudio]+1));

    audioOut = audioIn = info;

    VERBOSE(VB_AUDIO, LOC + "Audio format changed " +
            QString("\n\t\t\tfrom %1 to %2")
            .arg(old_in.toString()).arg(audioOut.toString()));

    if (audioOut.sample_rate > 0)
        m_audio->SetEffDsp(audioOut.sample_rate * 100);
    m_audio->SetAudioParams(audioOut.format, orig_channels, ctx->request_channels,
                            audioOut.codec_id, audioOut.sample_rate,
                            audioOut.do_passthru);
    m_audio->ReinitAudio();

    if (LCD *lcd = LCD::Get())
    {
        LCDAudioFormatSet audio_format;

        switch (ctx->codec_id)
        {
            case CODEC_ID_MP2:
                audio_format = AUDIO_MPEG2;
                break;
            case CODEC_ID_MP3:
                audio_format = AUDIO_MP3;
                break;
            case CODEC_ID_AC3:
                audio_format = AUDIO_AC3;
                break;
            case CODEC_ID_DTS:
                audio_format = AUDIO_DTS;
                break;
            case CODEC_ID_VORBIS:
                audio_format = AUDIO_OGG;
                break;
            case CODEC_ID_WMAV1:
                audio_format = AUDIO_WMA;
                break;
            case CODEC_ID_WMAV2:
                audio_format = AUDIO_WMA2;
                break;
            default:
                audio_format = AUDIO_WAV;
                break;
        }

        lcd->setAudioFormatLEDs(audio_format, true);

        if (audioOut.do_passthru)
            lcd->setVariousLEDs(VARIOUS_SPDIF, true);
        else
            lcd->setVariousLEDs(VARIOUS_SPDIF, false);

        switch (audioIn.channels)
        {
            case 0:
            /* nb: aac and mp3 seem to be coming up 0 here, may point to an
             * avformatdecoder audio channel handling bug, per janneg */
            case 1:
            case 2:
                /* all audio codecs have at *least* one channel, but
                 * LR is the fewest LED we can light up */
                lcd->setSpeakerLEDs(SPEAKER_LR, true);
                break;
            case 3:
            case 4:
            case 5:
            case 6:
                lcd->setSpeakerLEDs(SPEAKER_51, true);
                break;
            default:
                lcd->setSpeakerLEDs(SPEAKER_71, true);
                break;
        }

    }
    return true;
}

static int encode_frame(bool dts, unsigned char *data, int len,
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

    uint nr_samples = 0, block_len;
    if (dts)
    {
        enc_len = dts_syncinfo(data, &flags, &sample_rate, &bit_rate);
        if (enc_len < 0)
            return enc_len;
        int rate, sfreq, nblks;
        dts_decode_header(data, &rate, &nblks, &sfreq);
        nr_samples = nblks * 32;
        block_len = nr_samples * 2 * 2;
    }
    else
    {
        AC3HeaderInfo hdr;
        GetBitContext gbc;
        init_get_bits(&gbc, data, len * 8); // XXX HACK: assumes 8 bit per char
        if (!ff_ac3_parse_header(&gbc, &hdr))
        {
            enc_len = hdr.frame_size;
        }
        else
        {
            // creates endless loop
            enc_len = 0;
        }
        block_len = MAX_AC3_FRAME_SIZE;
    }

    if (enc_len == 0 || enc_len > len)
    {
        samples_size = 0;
        return len;
    }

    enc_len = min((uint)enc_len, block_len - 8);

    swab((const char*) data, (char*) (ucsamples + 8), enc_len);

    // the following values come from libmpcodecs/ad_hwac3.c in mplayer.
    // they form a valid IEC958 AC3 header.
    ucsamples[0] = 0x72;
    ucsamples[1] = 0xF8;
    ucsamples[2] = 0x1F;
    ucsamples[3] = 0x4E;
    ucsamples[4] = 0x01;
    if (dts)
    {
        switch(nr_samples)
        {
            case 512:
                ucsamples[4] = 0x0B;      /* DTS-1 (512-sample bursts) */
                break;

            case 1024:
                ucsamples[4] = 0x0C;      /* DTS-2 (1024-sample bursts) */
                break;

            case 2048:
                ucsamples[4] = 0x0D;      /* DTS-3 (2048-sample bursts) */
                break;

            default:
                VERBOSE(VB_IMPORTANT, LOC +
                        QString("DTS: %1-sample bursts not supported")
                        .arg(nr_samples));
                ucsamples[4] = 0x00;
                break;
        }
    }
    ucsamples[5] = 0x00;
    ucsamples[6] = (enc_len << 3) & 0xFF;
    ucsamples[7] = (enc_len >> 5) & 0xFF;
    memset(ucsamples + 8 + enc_len, 0, block_len - 8 - enc_len);
    samples_size = block_len;

    return enc_len;
}

static int DTS_SAMPLEFREQS[16] =
{
    0,      8000,   16000,  32000,  64000,  128000, 11025,  22050,
    44100,  88200,  176400, 12000,  24000,  48000,  96000,  192000
};

static int DTS_BITRATES[30] =
{
    32000,    56000,    64000,    96000,    112000,   128000,
    192000,   224000,   256000,   320000,   384000,   448000,
    512000,   576000,   640000,   768000,   896000,   1024000,
    1152000,  1280000,  1344000,  1408000,  1411200,  1472000,
    1536000,  1920000,  2048000,  3072000,  3840000,  4096000
};

static int dts_syncinfo(uint8_t *indata_ptr, int */*flags*/,
                        int *sample_rate, int *bit_rate)
{
    int nblks;
    int rate;
    int sfreq;

    int fsize = dts_decode_header(indata_ptr, &rate, &nblks, &sfreq);
    if (fsize >= 0)
    {
        if (rate >= 0 && rate <= 29)
            *bit_rate = DTS_BITRATES[rate];
        else
            *bit_rate = 0;
        if (sfreq >= 1 && sfreq <= 15)
            *sample_rate = DTS_SAMPLEFREQS[sfreq];
        else
            *sample_rate = 0;
    }
    return fsize;
}

// defines from libavcodec/dca.h
#define DCA_MARKER_RAW_BE 0x7FFE8001
#define DCA_MARKER_RAW_LE 0xFE7F0180
#define DCA_MARKER_14B_BE 0x1FFFE800
#define DCA_MARKER_14B_LE 0xFF1F00E8
#define DCA_HD_MARKER     0x64582025

static int dts_decode_header(uint8_t *indata_ptr, int *rate,
                             int *nblks, int *sfreq)
{
    uint id = ((indata_ptr[0] << 24) | (indata_ptr[1] << 16) |
               (indata_ptr[2] << 8)  | (indata_ptr[3]));

    switch (id)
    {
        case DCA_MARKER_RAW_BE:
            break;
        case DCA_MARKER_RAW_LE:
        case DCA_MARKER_14B_BE:
        case DCA_MARKER_14B_LE:
        case DCA_HD_MARKER:
            VERBOSE(VB_AUDIO+VB_EXTRA, LOC +
                    QString("DTS: Unsupported frame (id 0x%1)").arg(id, 8, 16));
            return -1;
            break;
        default:
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("DTS: Unknown frame (id 0x%1)").arg(id, 8, 16));
            return -1;
    }

    int ftype = indata_ptr[4] >> 7;

    int surp = (indata_ptr[4] >> 2) & 0x1f;
    surp = (surp + 1) % 32;

    *nblks = (indata_ptr[4] & 0x01) << 6 | (indata_ptr[5] >> 2);
    ++*nblks;

    int fsize = (indata_ptr[5] & 0x03) << 12 |
                (indata_ptr[6]         << 4) | (indata_ptr[7] >> 4);
    ++fsize;

    *sfreq = (indata_ptr[8] >> 2) & 0x0f;
    *rate = (indata_ptr[8] & 0x03) << 3 | ((indata_ptr[9] >> 5) & 0x07);

    if (ftype != 1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: Termination frames not handled (ftype %1)")
                .arg(ftype));
        return -1;
    }

    if (*sfreq != 13)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: Only 48kHz supported (sfreq %1)").arg(*sfreq));
        return -1;
    }

    if ((fsize > 8192) || (fsize < 96))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: fsize: %1 invalid").arg(fsize));
        return -1;
    }

    if (*nblks != 8 && *nblks != 16 && *nblks != 32 &&
        *nblks != 64 && *nblks != 128 && ftype == 1)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("DTS: nblks %1 not valid for normal frame")
                .arg(*nblks));
        return -1;
    }

    return fsize;
}

void AvFormatDecoder::av_update_stream_timings_video(AVFormatContext *ic)
{
    int64_t start_time, start_time1, end_time, end_time1;
    int64_t duration, duration1;
    AVStream *st = NULL;

    start_time = INT64_MAX;
    end_time = INT64_MIN;

    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st1 = ic->streams[i];
        if (st1 && st1->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }
    if (!st)
        return;

   duration = INT64_MIN;
   if (st->start_time != (int64_t)AV_NOPTS_VALUE && st->time_base.den) {
       start_time1= av_rescale_q(st->start_time, st->time_base, AV_TIME_BASE_Q);
       if (start_time1 < start_time)
           start_time = start_time1;
       if (st->duration != (int64_t)AV_NOPTS_VALUE) {
           end_time1 = start_time1
                     + av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
           if (end_time1 > end_time)
               end_time = end_time1;
       }
   }
   if (st->duration != (int64_t)AV_NOPTS_VALUE) {
       duration1 = av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
       if (duration1 > duration)
           duration = duration1;
   }
    if (start_time != INT64_MAX) {
        ic->start_time = start_time;
        if (end_time != INT64_MIN) {
            if (end_time - start_time > duration)
                duration = end_time - start_time;
        }
    }
    if (duration != INT64_MIN) {
        ic->duration = duration;
        if (ic->file_size > 0) {
            /* compute the bitrate */
            ic->bit_rate = (double)ic->file_size * 8.0 * AV_TIME_BASE /
                (double)ic->duration;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
