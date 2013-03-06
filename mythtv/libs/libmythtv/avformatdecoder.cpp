// C headers
#include <cassert>
#include <unistd.h>
#include <cmath>
#include <stdint.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

#include <QTextCodec>

// MythTV headers
#include "mythtvexp.h"
#include "mythconfig.h"
#include "avformatdecoder.h"
#include "privatedecoder.h"
#include "audiooutput.h"
#include "audiooutpututil.h"
#include "ringbuffer.h"
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
#include "videodisplayprofile.h"
#include "mythuihelper.h"
#include "DVD/dvdringbuffer.h"
#include "Bluray/bdringbuffer.h"

#include "lcddevice.h"

#include "videoout_quartz.h"  // For VOQ::GetBestSupportedCodec()

#ifdef USING_VDPAU
#include "videoout_vdpau.h"
extern "C" {
#include "libavcodec/vdpau.h"
}
#endif // USING_VDPAU

#ifdef USING_DXVA2
#include "videoout_d3d.h"
#endif

#ifdef USING_GLVAAPI
#include "videoout_openglvaapi.h"
#endif // USING_GLVAAPI
#ifdef USING_VAAPI
#include "vaapicontext.h"
#endif

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/mpegvideo.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavformat/internal.h"
#include "libswscale/swscale.h"
#include "libavformat/isom.h"
#include "ivtv_myth.h"
}

#ifdef _MSC_VER
// MSVC isn't C99 compliant...
# ifdef AV_TIME_BASE_Q
#  undef AV_TIME_BASE_Q
# endif
#define AV_TIME_BASE_Q  GetAVTimeBaseQ()

__inline AVRational GetAVTimeBaseQ()
{
    AVRational av = {1, AV_TIME_BASE};
    return av;
}
#endif

#define LOC QString("AFD: ")

#define MAX_AC3_FRAME_SIZE 6144

static const float eps = 1E-5;

static const int max_video_queue_size = 220;

static int cc608_parity(uint8_t byte);
static int cc608_good_parity(const int *parity_table, uint16_t data);
static void cc608_build_parity_table(int *parity_table);

static bool silence_ffmpeg_logging = false;

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
static float get_aspect(H264Parser &p)
{
    static const float default_aspect = 4.0f / 3.0f;
    int asp = p.aspectRatio();
    switch (asp)
    {
        case 0: return default_aspect;
        case 2: return 4.0f / 3.0f;
        case 3: return 16.0f / 9.0f;
        case 4: return 2.21f;
        default: break;
    }

    float aspect_ratio = asp * 0.000001f;
    if (aspect_ratio <= 0.0f || aspect_ratio > 6.0f)
    {
        if (p.pictureHeight() && p.pictureWidth())
        {
            aspect_ratio =
                (float) p.pictureWidth() /(float) p.pictureHeight();
        }
        else
        {
            aspect_ratio = default_aspect;
        }
    }
    return aspect_ratio;
}


int  get_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer(struct AVCodecContext *c, AVFrame *pic);
int  get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void render_slice_vdpau(struct AVCodecContext *s, const AVFrame *src,
                        int offset[4], int y, int type, int height);
int  get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic);
int  get_avf_buffer_vaapi(struct AVCodecContext *c, AVFrame *pic);

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
    if (silence_ffmpeg_logging)
        return;

    if (VERBOSE_LEVEL_NONE)
        return;

    static QString full_line("");
    static const int msg_len = 255;
    static QMutex string_lock;
    uint64_t   verbose_mask  = VB_GENERAL;
    LogLevel_t verbose_level = LOG_DEBUG;

    // determine mythtv debug level from av log level
    switch (level)
    {
        case AV_LOG_PANIC:
            verbose_level = LOG_EMERG;
            break;
        case AV_LOG_FATAL:
            verbose_level = LOG_CRIT;
            break;
        case AV_LOG_ERROR:
            verbose_level = LOG_ERR;
            verbose_mask |= VB_LIBAV;
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
        case AV_LOG_INFO:
            verbose_level = LOG_DEBUG;
            verbose_mask |= VB_LIBAV;
            break;
        case AV_LOG_WARNING:
            verbose_mask |= VB_LIBAV;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
        return;

    string_lock.lock();
    if (full_line.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        full_line.sprintf("[%s @ %p] ", avc->item_name(ptr), avc);
    }

    char str[msg_len+1];
    int bytes = vsnprintf(str, msg_len+1, fmt, vl);

    // check for truncated messages and fix them
    if (bytes > msg_len)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Libav log output truncated %1 of %2 bytes written")
                .arg(msg_len).arg(bytes));
        str[msg_len-1] = '\n';
    }

    full_line += QString(str);
    if (full_line.endsWith("\n"))
    {
        LOG(verbose_mask, verbose_level, full_line.trimmed());
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

#ifdef USING_VDPAU
    opts.decoders->append("vdpau");
    (*opts.equiv_decoders)["vdpau"].append("dummy");
#endif
#ifdef USING_DXVA2
    opts.decoders->append("dxva2");
    (*opts.equiv_decoders)["dxva2"].append("dummy");
#endif

#ifdef USING_VAAPI
    opts.decoders->append("vaapi");
    (*opts.equiv_decoders)["vaapi"].append("dummy");
#endif

    PrivateDecoder::GetDecoders(opts);
}

AvFormatDecoder::AvFormatDecoder(MythPlayer *parent,
                                 const ProgramInfo &pginfo,
                                 PlayerFlags flags)
    : DecoderBase(parent, pginfo),
      private_dec(NULL),
      is_db_ignored(gCoreContext->IsDatabaseIgnored()),
      m_h264_parser(new H264Parser()),
      ic(NULL),
      frame_decoded(0),             decoded_video_frame(NULL),
      avfRingBuffer(NULL),          sws_ctx(NULL),
      directrendering(false),
      no_dts_hack(false),           dorewind(false),
      gopset(false),                seen_gop(false),
      seq_count(0),
      prevgoppos(0),                gotVideoFrame(false),
      hasVideo(false),              needDummyVideoFrames(false),
      skipaudio(false),             allowedquit(false),
      start_code_state(0xffffffff),
      lastvpts(0),                  lastapts(0),
      lastccptsu(0),
      firstvpts(0),                 firstvptsinuse(false),
      faulty_pts(0),                faulty_dts(0),
      last_pts_for_fault_detection(0),
      last_dts_for_fault_detection(0),
      pts_detected(false),
      reordered_pts_detected(false),
      pts_selected(true),
      force_dts_timestamps(false),
      playerFlags(flags),
      video_codec_id(kCodec_NONE),
      maxkeyframedist(-1),
      // Closed Caption & Teletext decoders
      ignore_scte(false),
      invert_scte_field(0),
      last_scte_field(0),
      ccd608(new CC608Decoder(parent->GetCC608Reader())),
      ccd708(new CC708Decoder(parent->GetCC708Reader())),
      ttd(new TeletextDecoder(parent->GetTeletextReader())),
      // Interactive TV
      itv(NULL),
      // Audio
      disable_passthru(false),
      m_fps(0.0f),
      codec_is_mpeg(false),
      m_processFrames(true)
{
    memset(&readcontext, 0, sizeof(readcontext));
    memset(ccX08_in_pmt, 0, sizeof(ccX08_in_pmt));
    memset(ccX08_in_tracks, 0, sizeof(ccX08_in_tracks));

    audioSamples = (uint8_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                         sizeof(int32_t));
    ccd608->SetIgnoreTimecode(true);

    bool debug = VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY);
    av_log_set_level((debug) ? AV_LOG_DEBUG : AV_LOG_ERROR);
    av_log_set_callback(myth_av_log);

    audioIn.sample_size = -32; // force SetupAudioStream to run once
    itv = m_parent->GetInteractiveTV();

    cc608_build_parity_table(cc608_parity_table);

    SetIdrOnlyKeyframes(true);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("PlayerFlags: 0x%1")
        .arg(playerFlags, 0, 16));
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

    av_free(audioSamples);

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
        avformat_close_input(&ic);
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
        if (st1 && st1->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }
    if (!st)
        return false;

    if (ic->start_time != (int64_t)AV_NOPTS_VALUE)
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

    if (ic->start_time != (int64_t)AV_NOPTS_VALUE)
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
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
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
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
        long long framenum = (long long)(total_secs * fps);
        if (framesPlayed >= framenum)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
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
    long double total_secs = (long double)start * (long double)num /
                             (long double)den;
    long long framenum = (long long)(total_secs * fps);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("GetChapter %1: framenum %2")
                                   .arg(chapter).arg(framenum));
    return framenum;
}

bool AvFormatDecoder::DoRewind(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DoRewind(%1, %2 discard frames)")
            .arg(desiredFrame).arg( discardFrames ? "do" : "don't" ));

    if (recordingHasPositionMap || livetv)
        return DecoderBase::DoRewind(desiredFrame, discardFrames);

    dorewind = true;

    // avformat-based seeking
    return DoFastForward(desiredFrame, discardFrames);
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
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
        if (st1 && st1->codec->codec_type == AVMEDIA_TYPE_VIDEO)
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
        return true;
    }

    long long ts = 0;
    if (ic->start_time != (int64_t)AV_NOPTS_VALUE)
        ts = ic->start_time;

    // convert framenumber to normalized timestamp
    long double seekts = desiredFrame * AV_TIME_BASE / fps;
    ts += (long long)seekts;

    // XXX figure out how to do snapping in this case
    bool exactseeks = !DecoderBase::GetSeekSnap();

    int flags = (dorewind || exactseeks) ? AVSEEK_FLAG_BACKWARD : 0;

    if (av_seek_frame(ic, -1, ts, flags) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_seek_frame(ic, -1, %1, 0) -- error").arg(ts));
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
                                (int64_t)AV_TIME_BASE *
                                (int64_t)st->time_base.num,
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
        LOG(VB_GENERAL, LOG_INFO, LOC + "No DTS Seeking Hack!");
        no_dts_hack = true;
        framesPlayed = desiredFrame;
        framesRead = desiredFrame;
        normalframes = 0;
    }

    SeekReset(lastKey, normalframes, true, discardFrames);

    if (discardFrames)
        m_parent->SetFramesPlayed(framesPlayed + 1);

    dorewind = false;

    getrawframes = oldrawstate;

    return true;
}

void AvFormatDecoder::SeekReset(long long newKey, uint skipFrames,
                                bool doflush, bool discardFrames)
{
    if (!ringBuffer)
        return; // nothing to reset...

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
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
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "SeekReset() flushing");
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
        if (!ringBuffer->IsDVD())
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
        {
            m_parent->DiscardVideoFrame(decoded_video_frame);
            decoded_video_frame = NULL;
        }
    }

    if (doflush)
    {
        firstvpts = 0;
        firstvptsinuse = true;
    }
}

void AvFormatDecoder::SetEof(bool eof)
{
    if (!eof && ic && ic->pb)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("Resetting byte context eof (livetv %1 was eof %2)")
                .arg(livetv).arg(ic->pb->eof_reached));
        ic->pb->eof_reached = 0;
    }
    DecoderBase::SetEof(eof);
}

void AvFormatDecoder::Reset(bool reset_video_data, bool seek_reset,
                            bool reset_file)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Reset: Video %1, Seek %2, File %3")
            .arg(reset_video_data).arg(seek_reset).arg(reset_file));

    if (seek_reset)
        SeekReset(0, 0, true, false);

    DecoderBase::Reset(reset_video_data, false, reset_file);

    if (reset_video_data)
    {
        seen_gop = false;
        seq_count = 0;
    }
}

bool AvFormatDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize],
                                const QString &filename, int testbufsize)
{
    {
        QMutexLocker locker(avcodeclock);
        av_register_all();
    }

    AVProbeData probe;

    QByteArray fname = filename.toLatin1();
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
    int buf_size                = ringBuffer->BestBufferSize();
    int streamed                = ringBuffer->IsStreamed();
    readcontext.prot            = AVFRingBuffer::GetRingBufferURLProtocol();
    readcontext.flags           = AVIO_FLAG_READ;
    readcontext.is_streamed     = streamed;
    readcontext.max_packet_size = 0;
    readcontext.priv_data       = avfRingBuffer;
    unsigned char* buffer       = (unsigned char *)av_malloc(buf_size);
    ic->pb                      = avio_alloc_context(buffer, buf_size, 0,
                                                     &readcontext,
                                                     AVFRingBuffer::AVF_Read_Packet,
                                                     AVFRingBuffer::AVF_Write_Packet,
                                                     AVFRingBuffer::AVF_Seek_Packet);

    ic->pb->seekable            = !streamed;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Buffer size: %1, streamed %2")
        .arg(buf_size).arg(streamed));
}

extern "C" void HandleStreamChange(void *data)
{
    AvFormatDecoder *decoder =
        reinterpret_cast<AvFormatDecoder*>(data);

    int cnt = decoder->ic->nb_streams;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("streams_changed 0x%1 -- stream count %2")
            .arg((uint64_t)data,0,16).arg(cnt));

    QMutexLocker locker(avcodeclock);
    decoder->SeekReset(0, 0, true, true);
    decoder->ScanStreams(false);
}

extern "C" void HandleDVDStreamChange(void *data)
{
    AvFormatDecoder *decoder =
        reinterpret_cast<AvFormatDecoder*>(data);

    int cnt = decoder->ic->nb_streams;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("streams_changed 0x%1 -- stream count %2")
            .arg((uint64_t)data,0,16).arg(cnt));

    QMutexLocker locker(avcodeclock);
    //decoder->SeekReset(0, 0, true, true);
    decoder->ScanStreams(true);
}

extern "C" void HandleBDStreamChange(void *data)
{
    AvFormatDecoder *decoder =
        reinterpret_cast<AvFormatDecoder*>(data);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "resetting");

    QMutexLocker locker(avcodeclock);
    decoder->Reset(true, false, false);
    decoder->CloseCodecs();
    decoder->FindStreamInfo();
    decoder->ScanStreams(false);
}

int AvFormatDecoder::FindStreamInfo(void)
{
    QMutexLocker lock(avcodeclock);
    // Suppress ffmpeg logging unless "-v libav --loglevel debug"
    if (!VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_DEBUG))
        silence_ffmpeg_logging = true;
    avfRingBuffer->SetInInit(ringBuffer->IsStreamed());
    int retval = avformat_find_stream_info(ic, NULL);
    silence_ffmpeg_logging = false;
    avfRingBuffer->SetInInit(false);
    return retval;
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

    // Process frames immediately unless we're decoding
    // a DVD, in which case don't so that we don't show
    // anything whilst probing the data streams.
    m_processFrames = !ringBuffer->IsDVD();

    if (avfRingBuffer)
        delete avfRingBuffer;
    avfRingBuffer = new AVFRingBuffer(rbuffer);

    AVInputFormat *fmt      = NULL;
    QString        fnames   = ringBuffer->GetFilename();
    QByteArray     fnamea   = fnames.toLatin1();
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Probe failed for file: \"%1\".").arg(filename));
        return -1;
    }

#if 0
    fmt->flags |= AVFMT_NOFILE;
#endif

    ic = avformat_alloc_context();
    if (!ic)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not allocate format context.");
        return -1;
    }

    InitByteContext();

    int err = avformat_open_input(&ic, filename, fmt, NULL);
    if (err < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("avformat err(%1) on avformat_open_input call.").arg(err));
        return -1;
    }

    int ret = FindStreamInfo();
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not find codec parameters. " +
                QString("file was \"%1\".").arg(filename));
        avformat_close_input(&ic);
        ic = NULL;
        return -1;
    }
    ic->streams_changed = HandleStreamChange;
    if (ringBuffer->IsDVD())
        ic->streams_changed = HandleDVDStreamChange;
    else if (ringBuffer->IsBD())
        ic->streams_changed = HandleBDStreamChange;

    ic->stream_change_data = this;

    fmt->flags &= ~AVFMT_NOFILE;

    if (!livetv && !ringBuffer->IsDisc())
    {
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
    if (!recordingHasPositionMap && !is_db_ignored)
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

    // If watching pre-recorded television or video use the marked duration
    // from the db if it exists, else ffmpeg duration
    int64_t dur = 0;

    if (m_playbackinfo)
    {
        dur = m_playbackinfo->QueryTotalDuration();
        dur /= 1000000;
    }

    if (dur == 0)
    {
        if ((ic->duration == (int64_t)AV_NOPTS_VALUE) &&
            (!livetv && !ringBuffer->IsDisc()))
            av_estimate_timings(ic, 0);

        dur = ic->duration / (int64_t)AV_TIME_BASE;
    }

    if (dur > 0 && !livetv && !watchingrecording)
    {
        m_parent->SetDuration((int)dur);
    }

    // If we don't have a position map, set up ffmpeg for seeking
    if (!recordingHasPositionMap && !livetv)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
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

    av_dump_format(ic, 0, filename, 0);

    // print some useful information if playback debugging is on
    if (hasFullPositionMap)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Position map found");
    else if (recordingHasPositionMap)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Partial position map found");
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Successfully opened decoder for file: \"%1\". novideo(%2)")
            .arg(filename).arg(novideo));

    // Print AVChapter information
    for (unsigned int i=0; i < ic->nb_chapters; i++)
    {
        int num = ic->chapters[i]->time_base.num;
        int den = ic->chapters[i]->time_base.den;
        int64_t start = ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
        int hours = (int)total_secs / 60 / 60;
        int minutes = ((int)total_secs / 60) - (hours * 60);
        double secs = (double)total_secs -
                      (double)(hours * 60 * 60 + minutes * 60);
        long long framenum = (long long)(total_secs * fps);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Chapter %1 found @ [%2:%3:%4]->%5")
                .arg(QString().sprintf("%02d", i + 1))
                .arg(QString().sprintf("%02d", hours))
                .arg(QString().sprintf("%02d", minutes))
                .arg(QString().sprintf("%06.3f", secs))
                .arg(framenum));
    }

    if (getenv("FORCE_DTS_TIMESTAMPS"))
        force_dts_timestamps = true;

    if (ringBuffer->IsDVD())
    {
        // Reset DVD playback and clear any of
        // our buffers so that none of the data
        // parsed so far to determine decoders
        // gets shown.
        if (!ringBuffer->StartFromBeginning())
            return -1;
        ringBuffer->IgnoreWaitStates(false);

        Reset(true, true, true);

        // Now we're ready to process and show frames
        m_processFrames = true;
    }


    // Return true if recording has position map
    return recordingHasPositionMap;
}

float AvFormatDecoder::normalized_fps(AVStream *stream, AVCodecContext *enc)
{
    float fps, avg_fps, codec_fps, container_fps, estimated_fps;
    avg_fps = codec_fps = container_fps = estimated_fps = 0.0f;

    if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
        avg_fps = av_q2d(stream->avg_frame_rate); // MKV default_duration

    if (enc->time_base.den && enc->time_base.num) // tbc
        codec_fps = 1.0f / av_q2d(enc->time_base) / enc->ticks_per_frame;
    // Some formats report fps waaay too high. (wrong time_base)
    if (codec_fps > 121.0f && (enc->time_base.den > 10000) &&
        (enc->time_base.num == 1))
    {
        enc->time_base.num = 1001;  // seems pretty standard
        if (av_q2d(enc->time_base) > 0)
            codec_fps = 1.0f / av_q2d(enc->time_base);
    }
    if (stream->time_base.den && stream->time_base.num) // tbn
        container_fps = 1.0f / av_q2d(stream->time_base);
    if (stream->r_frame_rate.den && stream->r_frame_rate.num) // tbr
        estimated_fps = av_q2d(stream->r_frame_rate);

    // matroska demuxer sets the default_duration to avg_frame_rate
    // mov,mp4,m4a,3gp,3g2,mj2 demuxer sets avg_frame_rate
    if ((QString(ic->iformat->name).contains("matroska") ||
        QString(ic->iformat->name).contains("mov")) &&
        avg_fps < 121.0f && avg_fps > 3.0f)
        fps = avg_fps;
    else if (QString(ic->iformat->name).contains("avi") &&
        container_fps < 121.0f && container_fps > 3.0f)
        fps = container_fps; // avi uses container fps for timestamps
    else if (codec_fps < 121.0f && codec_fps > 3.0f)
        fps = codec_fps;
    else if (container_fps < 121.0f && container_fps > 3.0f)
        fps = container_fps;
    else if (estimated_fps < 121.0f && estimated_fps > 3.0f)
        fps = estimated_fps;
    else if (avg_fps < 121.0f && avg_fps > 3.0f)
        fps = avg_fps;
    else
        fps = 30000.0f / 1001.0f; // 29.97 fps

    if (fps != m_fps)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Selected FPS is %1 (avg %2 codec %3 "
                    "container %4 estimated %5)").arg(fps).arg(avg_fps)
                .arg(codec_fps).arg(container_fps).arg(estimated_fps));
        m_fps = fps;
    }

    return fps;
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

static enum PixelFormat get_format_vdpau(struct AVCodecContext *avctx,
                                         const enum PixelFormat *fmt)
{
    int i = 0;

    for(i=0; fmt[i]!=PIX_FMT_NONE; i++)
        if (IS_VDPAU_PIX_FMT(fmt[i]))
            break;

    return fmt[i];
}

// Declared seperately to allow attribute
static enum PixelFormat get_format_dxva2(struct AVCodecContext *,
                                         const enum PixelFormat *) MUNUSED;

enum PixelFormat get_format_dxva2(struct AVCodecContext *avctx,
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

// Declared seperately to allow attribute
static enum PixelFormat get_format_vaapi(struct AVCodecContext *,
                                         const enum PixelFormat *) MUNUSED;

enum PixelFormat get_format_vaapi(struct AVCodecContext *avctx,
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
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InitVideoCodec() 0x%1 id(%2) type (%3).")
            .arg((uint64_t)enc,0,16)
            .arg(ff_codec_id_string(enc->codec_id))
            .arg(ff_codec_type_string(enc->codec_type)));

    if (ringBuffer && ringBuffer->IsDVD())
        directrendering = false;

    enc->opaque = (void *)this;
    enc->get_buffer = get_avf_buffer;
    enc->release_buffer = release_avf_buffer;
    enc->draw_horiz_band = NULL;
    enc->slice_flags = 0;

    enc->err_recognition = AV_EF_COMPLIANT;
    enc->workaround_bugs = FF_BUG_AUTODETECT;
    enc->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    enc->idct_algo = FF_IDCT_AUTO;
    enc->debug = 0;
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

    AVDictionaryEntry *metatag =
        av_dict_get(stream->metadata, "rotate", NULL, 0);
    if (metatag && metatag->value && QString("180") == metatag->value)
        video_inverted = true;

    if (CODEC_IS_VDPAU(codec))
    {
        enc->get_buffer      = get_avf_buffer_vdpau;
        enc->get_format      = get_format_vdpau;
        enc->release_buffer  = release_avf_buffer_vdpau;
        enc->draw_horiz_band = render_slice_vdpau;
        enc->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else if (CODEC_IS_DXVA2(codec, enc))
    {
        enc->get_buffer      = get_avf_buffer_dxva2;
        enc->get_format      = get_format_dxva2;
        enc->release_buffer  = release_avf_buffer;
    }
    else if (CODEC_IS_VAAPI(codec, enc))
    {
        enc->get_buffer      = get_avf_buffer_vaapi;
        enc->get_format      = get_format_vaapi;
        enc->release_buffer  = release_avf_buffer;
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
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Using software scaling to convert pixel format %1 for "
                    "codec %2").arg(enc->pix_fmt)
                .arg(ff_codec_id_string(enc->codec_id)));
    }

    if (FlagIsSet(kDecodeLowRes)    || FlagIsSet(kDecodeSingleThreaded) ||
        FlagIsSet(kDecodeFewBlocks) || FlagIsSet(kDecodeNoLoopFilter)   ||
        FlagIsSet(kDecodeNoDecode))
    {
        if ((AV_CODEC_ID_MPEG2VIDEO == codec->id) ||
            (AV_CODEC_ID_MPEG1VIDEO == codec->id))
        {
            if (FlagIsSet(kDecodeFewBlocks))
            {
                uint total_blocks = (enc->height+15) / 16;
                enc->skip_top     = (total_blocks+3) / 4;
                enc->skip_bottom  = (total_blocks+3) / 4;
            }

            if (FlagIsSet(kDecodeLowRes))
                enc->lowres = 2; // 1 = 1/2 size, 2 = 1/4 size
        }
        else if (AV_CODEC_ID_H264 == codec->id)
        {
            if (FlagIsSet(kDecodeNoLoopFilter))
            {
                enc->flags &= ~CODEC_FLAG_LOOP_FILTER;
                enc->skip_loop_filter = AVDISCARD_ALL;
            }
        }

        if (FlagIsSet(kDecodeNoDecode))
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
        current_aspect = get_aspect(*enc);

        if (!width || !height)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "InitVideoCodec invalid dimensions, resetting decoder.");
            width  = 640;
            height = 480;
            fps    = 29.97f;
            current_aspect = 4.0f / 3.0f;
        }

        m_parent->SetKeyframeDistance(keyframedist);
        m_parent->SetVideoParams(width, height, fps, kScan_Detect);
        if (LCD *lcd = LCD::Get())
        {
            LCDVideoFormatSet video_format;

            switch (enc->codec_id)
            {
                case AV_CODEC_ID_H263:
                case AV_CODEC_ID_MPEG4:
                case AV_CODEC_ID_MSMPEG4V1:
                case AV_CODEC_ID_MSMPEG4V2:
                case AV_CODEC_ID_MSMPEG4V3:
                case AV_CODEC_ID_H263P:
                case AV_CODEC_ID_H263I:
                    video_format = VIDEO_DIVX;
                    break;
                case AV_CODEC_ID_WMV1:
                case AV_CODEC_ID_WMV2:
                    video_format = VIDEO_WMV;
                    break;
#if 0
                case AV_CODEC_ID_XVID:
                    video_format = VIDEO_XVID;
                    break;
#endif
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
        LOG(VB_VBI, LOG_ERR, LOC +
            QString("VBI: Bad parity in EIA-608 data (%1)") .arg(data,0,16));
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
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
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

    desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
        pmt.StreamInfo(i), pmt.StreamInfoLength(i),
        DescriptorID::caption_service);

    const desc_list_t desc_list2 = MPEGDescriptor::ParseOnlyInclude(
        pmt.ProgramInfo(), pmt.ProgramInfoLength(),
        DescriptorID::caption_service);

    desc_list.insert(desc_list.end(), desc_list2.begin(), desc_list2.end());

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
            LOG(VB_GENERAL, LOG_ERR, LOC + "in_tracks key too small");
        }
        else
        {
            ccX08_in_tracks[key] = true;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("%1 caption service #%2 is in the %3 language.")
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
        if (pmt.StreamType(i) != StreamID::PrivData)
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
                int magazine = td.TeletextMagazineNum(k);
                if (magazine == 0)
                    magazine = 8;
                int pagenum  = td.TeletextPageNum(k);
                int lang_idx = (magazine << 8) | pagenum;
                StreamInfo si(av_index, language, lang_idx, 0, 0);
                if (type == 2 || type == 1)
                {
                    TrackType track = (type == 2) ? kTrackTypeTeletextCaptions :
                                                    kTrackTypeTeletextMenu;
                    tracks[track].push_back(si);
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Teletext stream #%1 (%2) is in the %3 language"
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
    AVDictionaryEntry *metatag =
        av_dict_get(ic->streams[av_stream_index]->metadata, "language", NULL,
                    0);
    bool forced =
      ic->streams[av_stream_index]->disposition & AV_DISPOSITION_FORCED;
    int lang = metatag ? get_canonical_lang(metatag->value) :
                         iso639_str3_to_key("und");
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Text Subtitle track #%1 is A/V stream #%2 "
                "and is in the %3 language(%4), forced=%5.")
                    .arg(tracks[kTrackTypeRawText].size()).arg(av_stream_index)
                    .arg(iso639_key_toName(lang)).arg(lang).arg(forced));
    StreamInfo si(av_stream_index, lang, 0, 0, 0, false, false, forced);
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
    bool unknownbitrate = false;
    int scanerror = 0;
    bitrate       = 0;

    tracks[kTrackTypeAttachment].clear();
    tracks[kTrackTypeAudio].clear();
    tracks[kTrackTypeSubtitle].clear();
    tracks[kTrackTypeTeletextCaptions].clear();
    tracks[kTrackTypeTeletextMenu].clear();
    tracks[kTrackTypeRawText].clear();
    if (!novideo)
    {
        // we will rescan video streams
        tracks[kTrackTypeVideo].clear();
        selectedTrack[kTrackTypeVideo].av_stream_index = -1;
        fps = 0;
    }
    map<int,uint> lang_sub_cnt;
    uint subtitleStreamCount = 0;
    map<int,uint> lang_aud_cnt;
    uint audioStreamCount = 0;

    if (ringBuffer && ringBuffer->IsDVD() &&
        ringBuffer->DVD()->AudioStreamsChanged())
    {
        ringBuffer->DVD()->AudioStreamsChanged(false);
        RemoveAudioStreams();
    }

    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVCodecContext *enc = ic->streams[i]->codec;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stream #%1, has id 0x%2 codec id %3, "
                    "type %4, bitrate %5 at 0x%6")
                .arg(i).arg((uint64_t)ic->streams[i]->id,0,16)
                .arg(ff_codec_id_string(enc->codec_id))
                .arg(ff_codec_type_string(enc->codec_type))
                .arg(enc->bit_rate).arg((uint64_t)ic->streams[i],0,16));

        switch (enc->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                //assert(enc->codec_id);
                if (!enc->codec_id)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Stream #%1 has an unknown video "
                                "codec id, skipping.").arg(i));
                    continue;
                }

                // ffmpeg does not return a bitrate for several codecs and
                // formats. Forcing it to 500000 ensures the ringbuffer does not
                // use optimisations for low bitrate (audio and data) streams.
                if (enc->bit_rate == 0)
                {
                    enc->bit_rate = 500000;
                    unknownbitrate = true;
                }
                bitrate += enc->bit_rate;

                break;
            }
            case AVMEDIA_TYPE_AUDIO:
            {
                if (enc->codec)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Warning, audio codec 0x%1 id(%2) "
                                "type (%3) already open, leaving it alone.")
                            .arg((uint64_t)enc,0,16)
                            .arg(ff_codec_id_string(enc->codec_id))
                            .arg(ff_codec_type_string(enc->codec_type)));
                }
                //assert(enc->codec_id);
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("codec %1 has %2 channels")
                        .arg(ff_codec_id_string(enc->codec_id))
                        .arg(enc->channels));

                bitrate += enc->bit_rate;
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (enc->codec_id == AV_CODEC_ID_DVB_TELETEXT)
                    ScanTeletextCaptions(i);
                if (enc->codec_id == AV_CODEC_ID_TEXT)
                    ScanRawTextCaptions(i);
                bitrate += enc->bit_rate;

                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("subtitle codec (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_DATA:
            {
                ScanTeletextCaptions(i);
                bitrate += enc->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("data codec (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_ATTACHMENT:
            {
                if (enc->codec_id == AV_CODEC_ID_TTF)
                   tracks[kTrackTypeAttachment].push_back(
                       StreamInfo(i, 0, 0, ic->streams[i]->id, 0));
                bitrate += enc->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Attachment codec (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
            default:
            {
                bitrate += enc->bit_rate;
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Unknown codec type (%1)")
                        .arg(ff_codec_type_string(enc->codec_type)));
                break;
            }
        }

        if (enc->codec_type != AVMEDIA_TYPE_AUDIO &&
            enc->codec_type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        // skip DVB teletext and text subs, there is no libavcodec decoder
        if (enc->codec_type == AVMEDIA_TYPE_SUBTITLE &&
           (enc->codec_id   == AV_CODEC_ID_DVB_TELETEXT ||
            enc->codec_id   == AV_CODEC_ID_TEXT))
            continue;

        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Looking for decoder for %1")
                .arg(ff_codec_id_string(enc->codec_id)));

        if (enc->codec_id == AV_CODEC_ID_PROBE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Probing of stream #%1 unsuccesful, ignoring.").arg(i));
            continue;
        }

        AVCodec *codec = avcodec_find_decoder(enc->codec_id);
        if (!codec)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not find decoder for codec (%1), ignoring.")
                    .arg(ff_codec_id_string(enc->codec_id)));

            // Nigel's bogus codec-debug. Dump the list of codecs & decoders,
            // and have one last attempt to find a decoder. This is usually
            // only caused by build problems, where libavcodec needs a rebuild
            if (VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY))
            {
                AVCodec *p = av_codec_next(NULL);
                int      i = 1;
                while (p)
                {
                    QString msg;

                    if (p->name[0] != '\0')
                        msg = QString("Codec %1:").arg(p->name);
                    else
                        msg = QString("Codec %1, null name,").arg(i);

                    if (p->decode == NULL)
                        msg += "decoder is null";

                    LOG(VB_LIBAV, LOG_INFO, LOC + msg);

                    if (p->id == enc->codec_id)
                    {
                        codec = p;
                        break;
                    }

                    LOG(VB_LIBAV, LOG_INFO, LOC +
                        QString("Codec 0x%1 != 0x%2") .arg(p->id, 0, 16)
                            .arg(enc->codec_id, 0, 16));
                    p = av_codec_next(p);
                    ++i;
                }
            }
            if (!codec)
                continue;
        }

        if (!enc->codec)
        {
            if (OpenAVCodec(enc, codec) < 0)
                continue;
        }

        if (enc->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            bool forced = ic->streams[i]->disposition & AV_DISPOSITION_FORCED;
            int lang = GetSubtitleLanguage(subtitleStreamCount, i);
            int lang_indx = lang_sub_cnt[lang]++;
            subtitleStreamCount++;

            tracks[kTrackTypeSubtitle].push_back(
                StreamInfo(i, lang, lang_indx, ic->streams[i]->id, 0, 0, false, false, forced));

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Subtitle track #%1 is A/V stream #%2 "
                        "and is in the %3 language(%4).")
                    .arg(tracks[kTrackTypeSubtitle].size()).arg(i)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }

        if (enc->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            int lang = GetAudioLanguage(audioStreamCount, i);
            AudioTrackType type = GetAudioTrackType(i);
            int channels  = ic->streams[i]->codec->channels;
            int lang_indx = lang_aud_cnt[lang]++;
            audioStreamCount++;

            if (ic->streams[i]->codec->avcodec_dual_language)
            {
                tracks[kTrackTypeAudio].push_back(
                    StreamInfo(i, lang, lang_indx, ic->streams[i]->id, channels,
                               false, false, false, type));
                lang_indx = lang_aud_cnt[lang]++;
                tracks[kTrackTypeAudio].push_back(
                    StreamInfo(i, lang, lang_indx, ic->streams[i]->id, channels,
                               true, false, false, type));
            }
            else
            {
                int logical_stream_id;
                if (ringBuffer && ringBuffer->IsDVD())
                {
                    logical_stream_id =
                        ringBuffer->DVD()->GetAudioTrackNum(ic->streams[i]->id);
                }
                else
                    logical_stream_id = ic->streams[i]->id;

                tracks[kTrackTypeAudio].push_back(
                   StreamInfo(i, lang, lang_indx, logical_stream_id, channels,
                              false, false, false, type));
            }

            LOG(VB_AUDIO, LOG_INFO, LOC +
                QString("Audio Track #%1, of type (%2) is A/V stream #%3 "
                        "and has %4 channels in the %5 language(%6).")
                    .arg(tracks[kTrackTypeAudio].size()).arg(toString(type))
                    .arg(i).arg(enc->channels)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }
    }

    // Now find best video track to play
    if (!novideo && ic)
    {
        for(;;)
        {
            AVCodec *codec = NULL;
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Trying to select best video track");

            /*
             *  Find the "best" stream in the file.
             *
             * The best stream is determined according to various heuristics as
             * the most likely to be what the user expects. If the decoder parameter
             * is non-NULL, av_find_best_stream will find the default decoder
             * for the stream's codec; streams for which no decoder can be found
             * are ignored.
             *
             * If av_find_best_stream returns successfully and decoder_ret is not NULL,
             * then *decoder_ret is guaranteed to be set to a valid AVCodec.
             */
            int selTrack = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                               -1, -1, &codec, 0);

            if (selTrack < 0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "No video track found/selected.");
                break;
            }

            AVCodecContext *enc = ic->streams[selTrack]->codec;
            StreamInfo si(selTrack, 0, 0, 0, 0);

            tracks[kTrackTypeVideo].push_back(si);
            selectedTrack[kTrackTypeVideo] = si;

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Selected track #%1 (id 0x%2 codec id %3, "
                        "type %4, bitrate %5 at 0x%6)")
                .arg(selTrack).arg((uint64_t)ic->streams[selTrack]->id,0,16)
                .arg(ff_codec_id_string(enc->codec_id))
                .arg(ff_codec_type_string(enc->codec_type))
                .arg(enc->bit_rate).arg((uint64_t)ic->streams[selTrack],0,16));

            codec_is_mpeg = CODEC_IS_FFMPEG_MPEG(enc->codec_id);

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

            video_codec_id = kCodec_NONE;
            int version = mpeg_version(enc->codec_id);
            if (version)
                video_codec_id = (MythCodecID)(kCodec_MPEG1 + version - 1);

            if (version)
            {
#if defined(USING_VDPAU)
                // HACK -- begin
                // Force MPEG2 decoder on MPEG1 streams.
                // Needed for broken transmitters which mark
                // MPEG2 streams as MPEG1 streams, and should
                // be harmless for unbroken ones.
                if (enc->codec_id == AV_CODEC_ID_MPEG1VIDEO)
                    enc->codec_id = AV_CODEC_ID_MPEG2VIDEO;
                // HACK -- end
#endif // USING_VDPAU
#ifdef USING_VDPAU
                MythCodecID vdpau_mcid;
                vdpau_mcid =
                    VideoOutputVDPAU::GetBestSupportedCodec(width, height, dec,
                                                            mpeg_version(enc->codec_id),
                                                            !FlagIsSet(kDecodeAllowGPU));

                if (vdpau_mcid >= video_codec_id)
                {
                    enc->codec_id = (CodecID) myth2av_codecid(vdpau_mcid);
                    video_codec_id = vdpau_mcid;
                }
#endif // USING_VDPAU
#ifdef USING_GLVAAPI
                MythCodecID vaapi_mcid;
                PixelFormat pix_fmt = PIX_FMT_YUV420P;
                vaapi_mcid =
                    VideoOutputOpenGLVAAPI::GetBestSupportedCodec(width, height, dec,
                                                                  mpeg_version(enc->codec_id),
                                                                  !FlagIsSet(kDecodeAllowGPU),
                                                                  pix_fmt);

                if (vaapi_mcid >= video_codec_id)
                {
                    enc->codec_id = (CodecID)myth2av_codecid(vaapi_mcid);
                    video_codec_id = vaapi_mcid;
                    if (FlagIsSet(kDecodeAllowGPU) &&
                        codec_is_vaapi(video_codec_id))
                    {
                        enc->pix_fmt = pix_fmt;
                    }
                }
#endif // USING_GLVAAPI
#ifdef USING_DXVA2
                MythCodecID dxva2_mcid;
                PixelFormat pix_fmt = PIX_FMT_YUV420P;
                dxva2_mcid = VideoOutputD3D::GetBestSupportedCodec(
                                                                   width, height, dec, mpeg_version(enc->codec_id),
                                                                   !FlagIsSet(kDecodeAllowGPU), pix_fmt);

                if (dxva2_mcid >= video_codec_id)
                {
                    enc->codec_id = (CodecID)myth2av_codecid(dxva2_mcid);
                    video_codec_id = dxva2_mcid;
                    if (FlagIsSet(kDecodeAllowGPU) &&
                        codec_is_dxva2(video_codec_id))
                    {
                        enc->pix_fmt = pix_fmt;
                    }
                }
#endif // USING_DXVA2
            }

            // default to mpeg2
            if (video_codec_id == kCodec_NONE)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Unknown video codec - defaulting to MPEG2");
                video_codec_id = kCodec_MPEG2;
            }

            if (enc->codec)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Warning, video codec 0x%1 id(%2) type (%3) "
                            "already open.")
                    .arg((uint64_t)enc,0,16)
                    .arg(ff_codec_id_string(enc->codec_id))
                    .arg(ff_codec_type_string(enc->codec_type)));
            }

            // Use a PrivateDecoder if allowed in playerFlags AND matched
            // via the decoder name
            private_dec = PrivateDecoder::Create(dec, playerFlags, enc);
            if (private_dec)
                thread_count = 1;

            if (!codec_is_std(video_codec_id))
                thread_count = 1;

            if (FlagIsSet(kDecodeSingleThreaded))
                thread_count = 1;

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Using %1 CPUs for decoding")
                .arg(HAVE_THREADS ? thread_count : 1));

            if (HAVE_THREADS)
                enc->thread_count = thread_count;

            InitVideoCodec(ic->streams[selTrack], enc, true);

            ScanATSCCaptionStreams(selTrack);
            UpdateATSCCaptionTracks();

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Using %1 for video decoding")
                .arg(GetCodecDecoderName()));

            if (codec_is_vdpau(video_codec_id) && !CODEC_IS_VDPAU(codec))
            {
                codec = find_vdpau_decoder(codec, enc->codec_id);
            }

            if (!enc->codec)
            {
                QMutexLocker locker(avcodeclock);

                if (OpenAVCodec(enc, codec) < 0)
                {
                    scanerror = -1;
                    break;
                }
            }

            break;
        }
    }

    if (ic && ((uint)ic->bit_rate > bitrate))
        bitrate = (uint)ic->bit_rate;

    if (bitrate > 0)
    {
        bitrate = (bitrate + 999) / 1000;
        if (ringBuffer)
            ringBuffer->UpdateRawBitrate(bitrate);
    }

    // update RingBuffer buffer size
    if (ringBuffer)
    {
        ringBuffer->SetBufferSizeFactors(unknownbitrate,
                        ic && QString(ic->iformat->name).contains("matroska"));
    }

    PostProcessTracks();

    // Select a new track at the next opportunity.
    ResetTracks();

    // We have to do this here to avoid the NVP getting stuck
    // waiting on audio.
    if (m_audio->HasAudioIn() && tracks[kTrackTypeAudio].empty())
    {
        m_audio->SetAudioParams(FORMAT_NONE, -1, -1, AV_CODEC_ID_NONE, -1, false);
        m_audio->ReinitAudio();
        if (ringBuffer && ringBuffer->IsDVD())
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
            m_parent->SetVideoParams(-1, -1, 29.97);
        }
        else
        {
            fps = 25.0;
            m_parent->SetVideoParams(-1, -1, 25.0);
        }
    }

    if (m_parent->IsErrored())
        scanerror = -1;

    ScanDSMCCStreams();

    return scanerror;
}

bool AvFormatDecoder::OpenAVCodec(AVCodecContext *avctx, const AVCodec *codec)
{
    QMutexLocker locker(avcodeclock);

    int ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];

        av_make_error_string(error, sizeof(error), ret);
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not open codec 0x%1, id(%2) type(%3) "
                    "ignoring. reason %4").arg((uint64_t)avctx,0,16)
            .arg(ff_codec_id_string(avctx->codec_id))
            .arg(ff_codec_type_string(avctx->codec_type))
            .arg(error));
        return false;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Opened codec 0x%1, id(%2) type(%3)")
            .arg((uint64_t)avctx,0,16)
            .arg(ff_codec_id_string(avctx->codec_id))
            .arg(ff_codec_type_string(avctx->codec_type)));
        return true;
    }
}

void AvFormatDecoder::UpdateFramesPlayed(void)
{
    return DecoderBase::UpdateFramesPlayed();
}

bool AvFormatDecoder::DoRewindSeek(long long desiredFrame)
{
    return DecoderBase::DoRewindSeek(desiredFrame);
}

void AvFormatDecoder::DoFastForwardSeek(long long desiredFrame, bool &needflush)
{
    DecoderBase::DoFastForwardSeek(desiredFrame, needflush);
    return;
}
///Returns TeleText language
int AvFormatDecoder::GetTeletextLanguage(uint lang_idx) const
{
    for (uint i = 0; i < (uint) tracks[kTrackTypeTeletextCaptions].size(); i++)
    {
        if (tracks[kTrackTypeTeletextCaptions][i].language_index == lang_idx)
        {
             return tracks[kTrackTypeTeletextCaptions][i].language;
        }
    }

    return iso639_str3_to_key("und");
}
/// Returns DVD Subtitle language
int AvFormatDecoder::GetSubtitleLanguage(uint subtitle_index, uint stream_index)
{
    (void)subtitle_index;
    AVDictionaryEntry *metatag =
        av_dict_get(ic->streams[stream_index]->metadata, "language", NULL, 0);
    return metatag ? get_canonical_lang(metatag->value) :
                     iso639_str3_to_key("und");
}

/// Return ATSC Closed Caption Language
int AvFormatDecoder::GetCaptionLanguage(TrackTypes trackType, int service_num)
{
    int ret = -1;
    for (uint i = 0; i < (uint) pmt_track_types.size(); i++)
    {
        if ((pmt_track_types[i] == trackType) &&
            (pmt_tracks[i].stream_id == service_num))
        {
            ret = pmt_tracks[i].language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    for (uint i = 0; i < (uint) stream_track_types.size(); i++)
    {
        if ((stream_track_types[i] == trackType) &&
            (stream_tracks[i].stream_id == service_num))
        {
            ret = stream_tracks[i].language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    return ret;
}

int AvFormatDecoder::GetAudioLanguage(uint audio_index, uint stream_index)
{
    return GetSubtitleLanguage(audio_index, stream_index);
}

AudioTrackType AvFormatDecoder::GetAudioTrackType(uint stream_index)
{
    AudioTrackType type = kAudioTypeNormal;
    AVStream *stream = ic->streams[stream_index];

    if (ic->cur_pmt_sect) // mpeg-ts
    {
        const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
        const PSIPTable psip(pes);
        const ProgramMapTable pmt(psip);
        switch (pmt.GetAudioType(stream_index))
        {
            case 0x01 :
                type = kAudioTypeCleanEffects;
                break;
            case 0x02 :
                type = kAudioTypeHearingImpaired;
                break;
            case 0x03 :
                type = kAudioTypeAudioDescription;
                break;
            case 0x00 :
            default:
                type = kAudioTypeNormal;
        }
    }
    else // all other containers
    {
        // We only support labelling/filtering of these two types for now
        if (stream->disposition & AV_DISPOSITION_VISUAL_IMPAIRED)
            type = kAudioTypeAudioDescription;
        else if (stream->disposition & AV_DISPOSITION_COMMENT)
            type = kAudioTypeCommentary;
        else if (stream->disposition & AV_DISPOSITION_HEARING_IMPAIRED)
            type = kAudioTypeHearingImpaired;
        else if (stream->disposition & AV_DISPOSITION_CLEAN_EFFECTS)
            type = kAudioTypeCleanEffects;
    }

    return type;
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid stream index passed to "
                    "SetupAudioStreamSubIndexes: %1").arg(streamIndex));

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

        LOG(VB_GENERAL, LOG_WARNING, LOC + msg);

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

    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    if (!frame)
        return -1;

    for (int i = 0; i < 3; i++)
    {
        pic->data[i]     = frame->buf + frame->offsets[i];
        pic->linesize[i] = frame->pitches[i];
    }

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

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
        if (st->codec->codec_type == AVMEDIA_TYPE_AUDIO)
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
    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame((VideoFrame*)pic->opaque);

    assert(pic->type == FF_BUFFER_TYPE_USER);

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    pic->data[0] = frame->buf;
    pic->data[1] = frame->priv[0];
    pic->data[2] = frame->priv[1];

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = frame;
    pic->type = FF_BUFFER_TYPE_USER;

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
    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame((VideoFrame*)pic->opaque);

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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "render_slice_vdpau called with bad avctx or src");
    }
}

int get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();
    for (int i = 0; i < 4; i++)
    {
        pic->data[i]     = NULL;
        pic->linesize[i] = 0;
    }
    pic->reordered_opaque = c->reordered_opaque;
    pic->opaque      = frame;
    pic->type        = FF_BUFFER_TYPE_USER;
    frame->pix_fmt   = c->pix_fmt;

#ifdef USING_DXVA2
    if (nd->GetPlayer())
    {
        static uint8_t *dummy[1] = { 0 };
        c->hwaccel_context =
            (dxva_context*)nd->GetPlayer()->GetDecoderContext(NULL, dummy[0]);
        pic->data[0] = (uint8_t*)frame->buf;
        pic->data[3] = (uint8_t*)frame->buf;
    }
#endif

    return 0;
}

int get_avf_buffer_vaapi(struct AVCodecContext *c, AVFrame *pic)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    pic->data[0]     = frame->buf;
    pic->data[1]     = NULL;
    pic->data[2]     = NULL;
    pic->data[3]     = NULL;
    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;
    pic->linesize[3] = 0;
    pic->opaque      = frame;
    pic->type        = FF_BUFFER_TYPE_USER;
    frame->pix_fmt   = c->pix_fmt;

#ifdef USING_VAAPI
    if (nd->GetPlayer())
        c->hwaccel_context = (vaapi_context*)nd->GetPlayer()->GetDecoderContext(frame->buf, pic->data[3]);
#endif

    return 0;
}

void AvFormatDecoder::DecodeDTVCC(const uint8_t *buf, uint len, bool scte)
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

        uint data1    = buf[3+(cur*3)];
        uint data2    = buf[4+(cur*3)];
        uint data     = (data2 << 8) | data1;
        uint cc_type  = cc_code & 0x03;
        uint field;

        if (!cc_valid)
        {
            if (cc_type >= 0x2)
                ccd708->decode_cc_null();
            continue;
        }

        if (scte || cc_type <= 0x1) // EIA-608 field-1/2
        {
            if (cc_type == 0x2)
            {
                // SCTE repeated field
                field = !last_scte_field;
                invert_scte_field = !invert_scte_field;
            }
            else
            {
                field = cc_type ^ invert_scte_field;
            }

            if (cc608_good_parity(cc608_parity_table, data))
            {
                // in film mode, we may start at the wrong field;
                // correct if XDS start/cont/end code is detected
                // (must be field 2)
                if (scte && field == 0 &&
                    (data1 & 0x7f) <= 0x0f && (data1 & 0x7f) != 0x00)
                {
                    if (cc_type == 1)
                        invert_scte_field = 0;
                    field = 1;

                    // flush decoder
                    ccd608->FormatCC(0, -1, -1);
                }

                had_608 = true;
                ccd608->FormatCCField(lastccptsu / 1000, field, data);

                last_scte_field = field;
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
    for (uint i = 0; i < 4; i++)
    {
        if (seen_608[i] && !ccX08_in_pmt[i])
        {
            if (0==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 1);
            else if (2==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 2);
            else
                lang = iso639_str3_to_key("und");

            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i+1, false/*easy*/, false/*wide*/);
            stream_tracks.push_back(si);
            stream_track_types.push_back(kTrackTypeCC608);
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
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "gopset not set, syncing positionMap");
            SyncPositionMap();
            if (tempKeyFrameDist > 0 && !livetv)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Initial key frame distance: %1.")
                        .arg(keyframedist));
                gopset       = true;
                reset_kfd    = true;
            }
        }
        else if (keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
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

#if 0
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
            QString("framesRead: %1 last_frame: %2 keyframedist: %3")
                .arg(framesRead) .arg(last_frame) .arg(keyframedist));
#endif

        // if we don't have an entry, fill it in with what we've just parsed
        if (framesRead > last_frame && keyframedist > 0)
        {
            long long startpos = pkt->pos;

            LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                QString("positionMap[ %1 ] == %2.")
                    .arg(framesRead).arg(startpos));

            PosMapEntry entry = {framesRead, framesRead, startpos};

            QMutexLocker locker(&m_positionMapLock);
            // Create a dummy positionmap entry for frame 0 so that
            // seeking will work properly.  (See
            // DecoderBase::FindPosition() which subtracts
            // DecoderBase::indexOffset from each frame number.)
            if (m_positionMap.empty())
            {
                PosMapEntry dur = {0, 0, 0};
                m_positionMap.push_back(dur);
            }
            m_positionMap.push_back(entry);
            if (trackTotalDuration)
            {
                m_frameToDurMap[framesRead] =
                    (int64_t)totalDuration.num * 1000.0 / totalDuration.den
                    + 0.5;
                m_durToFrameMap[m_frameToDurMap[framesRead]] = framesRead;
            }
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
        bufptr = avpriv_mpv_find_start_code(bufptr, bufend, &start_code_state);

        float aspect_override = -1.0f;
        if (ringBuffer->IsDVD())
        {
            if (start_code_state == SEQ_END_CODE)
                ringBuffer->DVD()->NewSequence(true);
            aspect_override = ringBuffer->DVD()->GetAspectOverride();
        }

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
            current_aspect = seq->aspect(context->codec_id ==
                                         AV_CODEC_ID_MPEG1VIDEO);
            if (aspect_override > 0.0f)
                current_aspect = aspect_override;
            float seqFPS = seq->fps();

            bool changed = (seqFPS > fps+0.01f) || (seqFPS < fps-0.01f);
            changed |= (width  != (uint)current_width );
            changed |= (height != (uint)current_height);

            if (changed)
            {
                m_parent->SetVideoParams(width, height, seqFPS, kScan_Detect);

                current_width  = width;
                current_height = height;
                fps            = seqFPS;

                if (private_dec)
                    private_dec->Reset();

                gopset = false;
                prevgoppos = 0;
                firstvpts = lastapts = lastvpts = lastccptsu = 0;
                firstvptsinuse = true;
                faulty_pts = faulty_dts = 0;
                last_pts_for_fault_detection = 0;
                last_dts_for_fault_detection = 0;
                pts_detected = false;
                reordered_pts_detected = false;

                // fps debugging info
                float avFPS = normalized_fps(stream, context);
                if ((seqFPS > avFPS+0.01f) || (seqFPS < avFPS-0.01f))
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("avFPS(%1) != seqFPS(%2)")
                            .arg(avFPS).arg(seqFPS));
                }
            }

            seq_count++;

            if (!seen_gop && seq_count > 1)
            {
                HandleGopStart(pkt, true);
                pkt->flags |= AV_PKT_FLAG_KEY;
            }
        }
        else if (GOP_START == start_code_state)
        {
            HandleGopStart(pkt, true);
            seen_gop = true;
            pkt->flags |= AV_PKT_FLAG_KEY;
        }
    }
}

// Returns the number of frame starts identified in the packet.
int AvFormatDecoder::H264PreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = stream->codec;
    const uint8_t  *buf     = pkt->data;
    const uint8_t  *buf_end = pkt->data + pkt->size;
    int num_frames = 0;

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
            if (pkt->flags & AV_PKT_FLAG_KEY)
                HandleGopStart(pkt, false);
            return 1;
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
                    ++num_frames;

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

        current_aspect = get_aspect(*m_h264_parser);
        uint  width  = m_h264_parser->pictureWidthCropped();
        uint  height = m_h264_parser->pictureHeightCropped();
        float seqFPS = m_h264_parser->frameRate();

        bool res_changed = ((width  != (uint)current_width) ||
                            (height != (uint)current_height));
        bool fps_changed = (seqFPS > fps+0.01f) || (seqFPS < fps-0.01f);

        if (fps_changed || res_changed)
        {
            m_parent->SetVideoParams(width, height, seqFPS, kScan_Detect);

            current_width  = width;
            current_height = height;
            fps            = seqFPS;

            gopset = false;
            prevgoppos = 0;
            firstvpts = lastapts = lastvpts = lastccptsu = 0;
            firstvptsinuse = true;
            faulty_pts = faulty_dts = 0;
            last_pts_for_fault_detection = 0;
            last_dts_for_fault_detection = 0;
            pts_detected = false;
            reordered_pts_detected = false;

            // fps debugging info
            float avFPS = normalized_fps(stream, context);
            if ((seqFPS > avFPS+0.01f) || (seqFPS < avFPS-0.01f))
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("avFPS(%1) != seqFPS(%2)")
                        .arg(avFPS).arg(seqFPS));
            }

            // HACK HACK HACK - begin

            // The ffmpeg H.264 decoder currently does not support
            // resolution changes when thread_count!=1, so we
            // close and re-open the codec for resolution changes.

            bool do_it = HAVE_THREADS && res_changed;
            for (uint i = 0; do_it && (i < ic->nb_streams); i++)
            {
                AVCodecContext *enc = ic->streams[i]->codec;
                if ((AVMEDIA_TYPE_VIDEO == enc->codec_type) &&
                    (kCodec_H264 == video_codec_id) &&
                    (enc->codec) && (enc->thread_count>1))
                {
                    QMutexLocker locker(avcodeclock);
                    const AVCodec *codec = enc->codec;
                    avcodec_close(enc);
                    int open_val = avcodec_open2(enc, codec, NULL);
                    if (open_val < 0)
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC +
                            QString("Could not re-open codec 0x%1, "
                                    "id(%2) type(%3) "
                                    "aborting. reason %4")
                            .arg((uint64_t)enc,0,16)
                            .arg(ff_codec_id_string(enc->codec_id))
                            .arg(ff_codec_type_string(enc->codec_type))
                            .arg(open_val));
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            QString("Re-opened codec 0x%1, id(%2) type(%3)")
                            .arg((uint64_t)enc,0,16)
                            .arg(ff_codec_id_string(enc->codec_id))
                            .arg(ff_codec_type_string(enc->codec_type)));
                    }
                }
            }

            // HACK HACK HACK - end
        }

        HandleGopStart(pkt, true);
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    return num_frames;
}

bool AvFormatDecoder::PreProcessVideoPacket(AVStream *curstream, AVPacket *pkt)
{
    AVCodecContext *context = curstream->codec;
    int num_frames = 1;

    if (CODEC_IS_FFMPEG_MPEG(context->codec_id))
    {
        MpegPreProcessPkt(curstream, pkt);
    }
    else if (CODEC_IS_H264(context->codec_id))
    {
        num_frames = H264PreProcessPkt(curstream, pkt);
    }
    else
    {
        if (pkt->flags & AV_PKT_FLAG_KEY)
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
        !(pkt->flags & AV_PKT_FLAG_KEY))
    {
        av_free_packet(pkt);
        return false;
    }

    framesRead += num_frames;

    if (trackTotalDuration)
    {
        // The ffmpeg libraries represent a frame interval of a
        // 59.94fps video as 1501/90000 seconds, when it should
        // actually be 1501.5/90000 seconds.
        AVRational pkt_dur = AVRationalInit(pkt->duration);
        pkt_dur = av_mul_q(pkt_dur, curstream->time_base);
        if (pkt_dur.num == 1501 && pkt_dur.den == 90000)
            pkt_dur = AVRationalInit(1001, 60000); // 1501.5/90000
        totalDuration = av_add_q(totalDuration, pkt_dur);
    }

    justAfterChange = false;

    if (exitafterdecoded)
        gotVideoFrame = 1;

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
        if (ringBuffer->IsDVD() && ringBuffer->DVD()->NeedsStillFrame())
            ret = avcodec_decode_video2(context, &mpa_pic, &gotpicture, pkt);
    }
    avcodeclock->unlock();

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown decoding error");
        return false;
    }

    if (!gotpicture)
    {
        return true;
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
    if (force_dts_timestamps)
    {
        if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
            pts = pkt->dts;
        pts_selected = false;
    }
    else if (ringBuffer->IsDVD())
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

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_DEBUG, LOC +
        QString("video packet timestamps reordered %1 pts %2 dts %3 (%4)")
            .arg(mpa_pic.reordered_opaque).arg(pkt->pts).arg(pkt->dts)
            .arg((force_dts_timestamps) ? "dts forced" :
                 (pts_selected) ? "reordered" : "dts"));

    mpa_pic.reordered_opaque = pts;

    ProcessVideoFrame(curstream, &mpa_pic);

    return true;
}

bool AvFormatDecoder::ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic)
{
    AVCodecContext *context = stream->codec;

    uint cc_len = (uint) max(mpa_pic->scte_cc_len,0);
    uint8_t *cc_buf = mpa_pic->scte_cc_buf;
    bool scte = true;

    // If both ATSC and SCTE caption data are available, prefer ATSC
    if ((mpa_pic->atsc_cc_len > 0) || ignore_scte)
    {
        ignore_scte = true;
        cc_len = (uint) max(mpa_pic->atsc_cc_len, 0);
        cc_buf = mpa_pic->atsc_cc_buf;
        scte = false;
    }

    // Decode CEA-608 and CEA-708 captions
    for (uint i = 0; i < cc_len; i += ((cc_buf[i] & 0x1f) * 3) + 2)
        DecodeDTVCC(cc_buf + i, cc_len - i, scte);

    VideoFrame *picframe = (VideoFrame *)(mpa_pic->opaque);

    if (FlagIsSet(kDecodeNoDecode))
    {
        // Do nothing, we just want the pts, captions, subtites, etc.
        // So we can release the unconverted blank video frame to the
        // display queue.
    }
    else if (!directrendering)
    {
        AVPicture tmppicture;

        VideoFrame *xf = picframe;
        picframe = m_parent->GetNextVideoFrame();

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
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate sws context");
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
            xf->aspect = current_aspect;
            m_parent->DiscardVideoFrame(xf);
        }
    }
    else if (!picframe)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "NULL videoframe - direct rendering not"
                                       "correctly initialized.");
        return false;
    }

    long long pts = (long long)(av_q2d(stream->time_base) *
                                mpa_pic->reordered_opaque * 1000);

    long long temppts = pts;
    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!ringBuffer->IsDVD() &&
        temppts <= lastvpts &&
        (temppts + (1000 / fps) > lastvpts || temppts <= 0))
    {
        temppts = lastvpts;
        temppts += (long long)(1000 / fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += (long long)(mpa_pic->repeat_pict * 500 / fps);
    }

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
        QString("video timecode %1 %2 %3 %4%5")
            .arg(mpa_pic->reordered_opaque).arg(pts).arg(temppts).arg(lastvpts)
            .arg((pts != temppts) ? " fixup" : ""));

    picframe->interlaced_frame = mpa_pic->interlaced_frame;
    picframe->top_field_first  = mpa_pic->top_field_first;
    picframe->repeat_pict      = mpa_pic->repeat_pict;
    picframe->disp_timecode    = NormalizeVideoTimecode(stream, temppts);
    picframe->frameNumber      = framesPlayed;
    picframe->aspect           = current_aspect;
    picframe->dummy            = 0;

    m_parent->ReleaseNextVideoFrame(picframe, temppts);
    if (private_dec)
        context->release_buffer(context, mpa_pic);

    decoded_video_frame = picframe;
    gotVideoFrame = 1;
    framesPlayed++;

    lastvpts = temppts;
    if (!firstvpts && firstvptsinuse)
        firstvpts = temppts;

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
        LOG(VB_VBI, LOG_ERR, LOC + QString("Unknown VBI data stream '%1%2%3'")
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
            LOG(VB_VBI, LOG_ERR, LOC +
                QString("VBI: Unknown descriptor: %1").arg(*buf));
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
    if (!m_parent->GetSubReader(pkt->stream_index))
        return true;

    long long pts = 0;

    if (pkt->dts != (int64_t)AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    avcodeclock->lock();
    int subIdx = selectedTrack[kTrackTypeSubtitle].av_stream_index;
    bool isForcedTrack = selectedTrack[kTrackTypeSubtitle].forced;
    avcodeclock->unlock();

    int gotSubtitles = 0;
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(AVSubtitle));

    if (ringBuffer->IsDVD())
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
    else if (decodeAllSubtitles || pkt->stream_index == subIdx)
    {
        QMutexLocker locker(avcodeclock);
        avcodec_decode_subtitle2(curstream->codec, &subtitle, &gotSubtitles,
                                 pkt);
    }

    if (gotSubtitles)
    {
        if (isForcedTrack)
            subtitle.forced = true;
        subtitle.start_display_time += pts;
        subtitle.end_display_time += pts;
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("subtl timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts)
                .arg(subtitle.start_display_time)
                .arg(subtitle.end_display_time));

        bool forcedon = m_parent->GetSubReader(pkt->stream_index)->AddAVSubtitle(
               subtitle, curstream->codec->codec_id == AV_CODEC_ID_XSUB,
               m_parent->GetAllowForcedSubtitles());
        m_parent->EnableForcedSubtitles(forcedon || isForcedTrack);
    }

    return true;
}

bool AvFormatDecoder::ProcessRawTextPacket(AVPacket *pkt)
{
    if (!(decodeAllSubtitles ||
        selectedTrack[kTrackTypeRawText].av_stream_index == pkt->stream_index))
    {
        return false;
    }

    if (!m_parent->GetSubReader(pkt->stream_index+0x2000))
        return false;

    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    QTextDecoder *dec = codec->makeDecoder();
    QString text      = dec->toUnicode((const char*)pkt->data, pkt->size);
    QStringList list  = text.split('\n', QString::SkipEmptyParts);
    delete dec;

    m_parent->GetSubReader(pkt->stream_index+0x2000)->
        AddRawTextSubtitle(list, pkt->convergence_duration);

    return true;
}

bool AvFormatDecoder::ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                        DecodeType decodetype)
{
    enum CodecID codec_id = curstream->codec->codec_id;

    switch (codec_id)
    {
        case AV_CODEC_ID_MPEG2VBI:
            ProcessVBIDataPacket(curstream, pkt);
            break;
        case AV_CODEC_ID_DVB_VBI:
            ProcessDVBDataPacket(curstream, pkt);
            break;
        case AV_CODEC_ID_DSMCC_B:
        {
            ProcessDSMCCPacket(curstream, pkt);
            GenerateDummyVideoFrames();
            // Have to return regularly to ensure that the OSD is updated.
            // This applies both to MHEG and also channel browsing.
#ifdef USING_MHEG
            if (!(decodetype & kDecodeVideo))
                allowedquit |= (itv && itv->ImageHasChanged());
#endif // USING_MHEG:
            break;
        }
        default:
            break;
    }
    return true;
}

int AvFormatDecoder::SetTrack(uint type, int trackNo)
{
    bool ret = DecoderBase::SetTrack(type, trackNo);

    if (kTrackTypeAudio == type)
    {
        QString msg = SetupAudioStream() ? "" : "not ";
        LOG(VB_AUDIO, LOG_INFO, LOC + "Audio stream type "+msg+"changed.");
    }

    return ret;
}

QString AvFormatDecoder::GetTrackDesc(uint type, uint trackNo) const
{
    if (trackNo >= tracks[type].size())
        return "";

    bool forced = tracks[type][trackNo].forced;
    int lang_key = tracks[type][trackNo].language;
    QString forcedString = forced ? QObject::tr(" (forced)") : "";
    if (kTrackTypeAudio == type)
    {
        if (ringBuffer->IsDVD())
            lang_key = ringBuffer->DVD()->GetAudioLanguage(trackNo);

        QString msg = iso639_key_toName(lang_key);

        switch (tracks[type][trackNo].audio_type)
        {
            case kAudioTypeNormal :
            {
                int av_index = tracks[kTrackTypeAudio][trackNo].av_stream_index;
                AVStream *s = ic->streams[av_index];

                if (s)
                {
                    if (s->codec->codec_id == AV_CODEC_ID_MP3)
                        msg += QString(" MP%1").arg(s->codec->sub_id);
                    else if (s->codec->codec)
                        msg += QString(" %1").arg(s->codec->codec->name).toUpper();

                    int channels = 0;
                    if (ringBuffer->IsDVD())
                        channels = ringBuffer->DVD()->GetNumAudioChannels(trackNo);
                    else if (s->codec->channels)
                        channels = tracks[kTrackTypeAudio][trackNo].orig_num_channels;

                    if (channels == 0)
                        msg += QString(" ?ch");
                    else if((channels > 4) && !(channels & 1))
                        msg += QString(" %1.1ch").arg(channels - 1);
                    else
                        msg += QString(" %1ch").arg(channels);
                }

                break;
            }
            case kAudioTypeAudioDescription :
            case kAudioTypeCommentary :
            case kAudioTypeHearingImpaired :
            case kAudioTypeCleanEffects :
            case kAudioTypeSpokenSubs :
            default :
                msg += QString(" (%1)")
                            .arg(toString(tracks[type][trackNo].audio_type));
                break;
        }

        return QString("%1: %2").arg(trackNo + 1).arg(msg);
    }
    else if (kTrackTypeSubtitle == type)
    {
        if (ringBuffer->IsDVD())
            lang_key = ringBuffer->DVD()->GetSubtitleLanguage(trackNo);

        return QObject::tr("Subtitle") + QString(" %1: %2%3")
            .arg(trackNo + 1).arg(iso639_key_toName(lang_key))
            .arg(forcedString);
    }
    else if (forced && kTrackTypeRawText == type)
    {
        return DecoderBase::GetTrackDesc(type, trackNo) + forcedString;
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

QByteArray AvFormatDecoder::GetSubHeader(uint trackNo) const
{
    if (trackNo >= tracks[kTrackTypeSubtitle].size())
        return QByteArray();

    int index = tracks[kTrackTypeSubtitle][trackNo].av_stream_index;
    if (!ic->streams[index]->codec)
        return QByteArray();

    return QByteArray((char *)ic->streams[index]->codec->subtitle_header,
                      ic->streams[index]->codec->subtitle_header_size);
}

void AvFormatDecoder::GetAttachmentData(uint trackNo, QByteArray &filename,
                                        QByteArray &data)
{
    if (trackNo >= tracks[kTrackTypeAttachment].size())
        return;

    int index = tracks[kTrackTypeAttachment][trackNo].av_stream_index;
    AVDictionaryEntry *tag = av_dict_get(ic->streams[index]->metadata,
                                         "filename", NULL, 0);
    if (tag)
        filename  = QByteArray(tag->value);
    data      = QByteArray((char *)ic->streams[index]->codec->extradata,
                           ic->streams[index]->codec->extradata_size);
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

    if (ringBuffer->IsInDiscMenuOrStillFrame())
        return -1;

    return DecoderBase::AutoSelectTrack(type);
}

static vector<int> filter_lang(const sinfo_vec_t &tracks, int lang_key,
                               const vector<int> ftype)
{
    vector<int> ret;

    vector<int>::const_iterator it = ftype.begin();
    for (; it != ftype.end(); ++it)
    {
        if ((lang_key < 0) || tracks[*it].language == lang_key)
            ret.push_back(*it);
    }

    return ret;
}

static vector<int> filter_type(const sinfo_vec_t &tracks, AudioTrackType type)
{
    vector<int> ret;

    for (uint i = 0; i < tracks.size(); i++)
    {
        if (tracks[i].audio_type == type)
            ret.push_back(i);
    }

    return ret;
}

int AvFormatDecoder::filter_max_ch(const AVFormatContext *ic,
                                   const sinfo_vec_t     &tracks,
                                   const vector<int>     &fs,
                                   enum CodecID           codecId,
                                   int                    profile)
{
    int selectedTrack = -1, max_seen = -1;

    vector<int>::const_iterator it = fs.begin();
    for (; it != fs.end(); ++it)
    {
        const int stream_index = tracks[*it].av_stream_index;
        const AVCodecContext *ctx = ic->streams[stream_index]->codec;
        if ((codecId == AV_CODEC_ID_NONE || codecId == ctx->codec_id) &&
            (max_seen < ctx->channels))
        {
            if (codecId == AV_CODEC_ID_DTS && profile > 0)
            {
                // we cannot decode dts-hd, so only select it if passthrough
                if (!DoPassThrough(ctx, true) || ctx->profile != profile)
                    continue;
            }
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
                       DoPassThrough(codec_ctx, true));
        LOG(VB_AUDIO, LOG_DEBUG, LOC + " * " + item.toString());
    }
#endif

    int selTrack = (1 == numStreams) ? 0 : -1;
    int wlang    = wtrack.language;


    if ((selTrack < 0) && (wtrack.av_substream_index >= 0))
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to reselect audio sub-stream");
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
        LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to reselect audio track");
        // Try to reselect user selected audio stream.
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
        LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to select audio track (w/lang)");

        // Filter out commentary and audio description tracks
        vector<int> ftype = filter_type(atracks, kAudioTypeNormal);

        if (ftype.empty())
        {
            LOG(VB_AUDIO, LOG_WARNING, "No audio tracks matched the type filter, "
                                       "so trying all tracks.");
            for (int i = 0; i < static_cast<int>(atracks.size()); i++)
                ftype.push_back(i);
        }

        // try to get the language track matching the frontend language.
        QString language_key_convert = iso639_str2_to_str3(gCoreContext->GetLanguage());
        uint language_key = iso639_str3_to_key(language_key_convert);
        uint canonical_key = iso639_key_to_canonical_key(language_key);

        vector<int> flang = filter_lang(atracks, canonical_key, ftype);

        if (m_audio->CanDTSHD())
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                     FF_PROFILE_DTS_HD_MA);
        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_TRUEHD);

        if (selTrack < 0 && m_audio->CanDTSHD())
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                     FF_PROFILE_DTS_HD_HRA);
        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_EAC3);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_AC3);

        if (selTrack < 0)
            selTrack = filter_max_ch(ic, atracks, flang);

        // try to get best track for most preferred language
        // Set by the "Guide Data" language prefs in Appearance.
        if (selTrack < 0)
        {
            vector<int>::const_iterator it = languagePreference.begin();
            for (; it !=  languagePreference.end() && selTrack < 0; ++it)
            {
                vector<int> flang = filter_lang(atracks, *it, ftype);

                if (m_audio->CanDTSHD())
                    selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                             FF_PROFILE_DTS_HD_MA);
                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang,
                                             AV_CODEC_ID_TRUEHD);

                if (selTrack < 0 && m_audio->CanDTSHD())
                    selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                             FF_PROFILE_DTS_HD_HRA);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang,
                                             AV_CODEC_ID_EAC3);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_AC3);

                if (selTrack < 0)
                    selTrack = filter_max_ch(ic, atracks, flang);
            }
        }

        // could not select track based on user preferences (language)
        // try to select the default track
        if (selTrack < 0 && numStreams)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to select default track");
            for (uint i = 0; i < atracks.size(); i++) {
                int idx = atracks[i].av_stream_index;
                if (ic->streams[idx]->disposition & AV_DISPOSITION_DEFAULT)
                {
                    selTrack = i;
                    break;
                }
            }
        }

        // try to get best track for any language
        if (selTrack < 0)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC +
                "Trying to select audio track (wo/lang)");
            vector<int> flang = filter_lang(atracks, -1, ftype);

            if (m_audio->CanDTSHD())
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                         FF_PROFILE_DTS_HD_MA);
            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_TRUEHD);

            if (selTrack < 0 && m_audio->CanDTSHD())
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS,
                                         FF_PROFILE_DTS_HD_HRA);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_EAC3);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_DTS);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang, AV_CODEC_ID_AC3);

            if (selTrack < 0)
                selTrack = filter_max_ch(ic, atracks, flang);
        }
    }

    if (selTrack < 0)
    {
        strack.av_stream_index = -1;
        if (ctrack != selTrack)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "No suitable audio track exists.");
            ctrack = selTrack;
        }
    }
    else
    {
        ctrack = selTrack;
        strack = atracks[selTrack];

        if (wtrack.av_stream_index < 0)
            wtrack = strack;

        LOG(VB_AUDIO, LOG_INFO, LOC +
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
    int ret             = 0;
    int data_size       = 0;
    bool firstloop      = true;
    int decoded_size    = -1;

    avcodeclock->lock();
    int audIdx = selectedTrack[kTrackTypeAudio].av_stream_index;
    int audSubIdx = selectedTrack[kTrackTypeAudio].av_substream_index;
    avcodeclock->unlock();

    AVPacket tmp_pkt;
    av_init_packet(&tmp_pkt);
    tmp_pkt.data = pkt->data;
    tmp_pkt.size = pkt->size;

    while (tmp_pkt.size > 0)
    {
        bool reselectAudioTrack = false;

        /// HACK HACK HACK -- begin See #3731
        if (!m_audio->HasAudioIn())
        {
            LOG(VB_AUDIO, LOG_INFO, LOC +
                "Audio is disabled - trying to restart it");
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
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Setting channels to %1")
                    .arg(audioOut.channels));

            QMutexLocker locker(avcodeclock);

            if (DoPassThrough(ctx, false) || !DecoderWillDownmix(ctx))
            {
                // for passthru or codecs for which the decoder won't downmix
                // let the decoder set the number of channels. For other codecs
                // we downmix if necessary in audiooutputbase
                ctx->request_channels = 0;
            }
            else // No passthru, the decoder will downmix
            {
                ctx->request_channels = m_audio->GetMaxChannels();
                if (ctx->codec_id == AV_CODEC_ID_AC3)
                    ctx->channels = m_audio->GetMaxChannels();
            }

            ret = AudioOutputUtil::DecodeAudio(ctx, audioSamples, data_size, &tmp_pkt);
            decoded_size = data_size;
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

        // skip any audio frames preceding first video frame
        if (firstvptsinuse && firstvpts && (lastapts < firstvpts))
        {
            LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                QString("discarding early audio timecode %1 %2 %3")
                    .arg(pkt->pts).arg(pkt->dts).arg(lastapts));
            break;
        }
        firstvptsinuse = false;

        avcodeclock->lock();
        data_size = 0;

        if (audioOut.do_passthru)
        {
            if (!already_decoded)
            {
                if (m_audio->NeedDecodingBeforePassthrough())
                {
                    ret = AudioOutputUtil::DecodeAudio(ctx, audioSamples, data_size, &tmp_pkt);
                    decoded_size = data_size;
                }
                else
                    decoded_size = -1;
            }
            memcpy(audioSamples, tmp_pkt.data, tmp_pkt.size);
            data_size = tmp_pkt.size;
            // We have processed all the data, there can't be any left
            tmp_pkt.size = 0;
        }
        else
        {
            if (!already_decoded)
            {
                if (DecoderWillDownmix(ctx))
                {
                    ctx->request_channels = m_audio->GetMaxChannels();
                    if (ctx->codec_id == AV_CODEC_ID_AC3)
                        ctx->channels = m_audio->GetMaxChannels();
                }
                else
                    ctx->request_channels = 0;

                ret = AudioOutputUtil::DecodeAudio(ctx, audioSamples, data_size, &tmp_pkt);
                decoded_size = data_size;
            }

            // When decoding some audio streams the number of
            // channels, etc isn't known until we try decoding it.
            if (ctx->sample_rate != audioOut.sample_rate ||
                ctx->channels    != audioOut.channels)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "Audio stream changed");
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
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown audio decoding error");
            return false;
        }

        if (data_size <= 0)
        {
            tmp_pkt.data += ret;
            tmp_pkt.size -= ret;
            continue;
        }

        long long temppts = lastapts;

        if (audSubIdx != -1 && !audioOut.do_passthru)
            extract_mono_channel(audSubIdx, &audioOut,
                                 (char *)audioSamples, data_size);

        int frames = (ctx->channels <= 0 || decoded_size < 0) ? -1 :
            decoded_size / (ctx->channels *
                            av_get_bytes_per_sample(ctx->sample_fmt));
        m_audio->AddAudioData((char *)audioSamples, data_size, temppts, frames);
        if (audioOut.do_passthru && !m_audio->NeedDecodingBeforePassthrough())
        {
            lastapts += m_audio->LengthLastData();
        }
        else
        {
            lastapts += (long long)
                ((double)(frames * 1000) / ctx->sample_rate);
        }

        LOG(VB_TIMESTAMP, LOG_INFO, LOC + QString("audio timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(lastapts));

        allowedquit |= ringBuffer->IsInStillFrame() ||
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

    const DecodeType origDecodetype = decodetype;

    gotVideoFrame = false;

    frame_decoded = 0;
    decoded_video_frame = NULL;

    allowedquit = false;
    bool storevideoframes = false;

    avcodeclock->lock();
    AutoSelectTracks();
    avcodeclock->unlock();

    skipaudio = (lastvpts == 0);

    if( !m_processFrames )
    {
        return false;
    }

    hasVideo = HasVideo(ic);
    needDummyVideoFrames = false;

    if (!hasVideo && (decodetype & kDecodeVideo))
    {
        // NB This could be an issue if the video stream is not
        // detected initially as the video buffers will be filled.
        needDummyVideoFrames = true;
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
        if (decodetype & kDecodeAudio)
        {
            if (((currentTrack[kTrackTypeAudio] < 0) ||
                 (selectedTrack[kTrackTypeAudio].av_stream_index < 0)))
            {
                // disable audio request if there are no audio streams anymore
                // and we have video, otherwise allow decoding to stop
                if (hasVideo)
                    decodetype = (DecodeType)((int)decodetype & ~kDecodeAudio);
                else
                    allowedquit = true;
            }
        }
        else
        if ((origDecodetype & kDecodeAudio) &&
            (currentTrack[kTrackTypeAudio] >= 0) &&
            (selectedTrack[kTrackTypeAudio].av_stream_index >= 0))
        {
            // Turn on audio decoding again if it was on originally
            // and an audio stream has now appeared.  This can happen
            // in still DVD menus with audio
            decodetype = (DecodeType)((int)decodetype | kDecodeAudio);
        }

        StreamChangeCheck();

        if (gotVideoFrame)
        {
            if (decodetype == kDecodeNothing)
            {
                // no need to buffer audio or video if we
                // only care about building a keyframe map.
                allowedquit = true;
                continue;
            }
            else if (lowbuffers && ((decodetype & kDecodeAV) == kDecodeAV) &&
                     (storedPackets.count() < max_video_queue_size) &&
                     (lastapts < lastvpts + 100) &&
                     !ringBuffer->IsInStillFrame())
            {
                storevideoframes = true;
            }
            else if (decodetype & kDecodeVideo)
            {
                if (storedPackets.count() >= max_video_queue_size)
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
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
                memset(pkt, 0, sizeof(AVPacket));
                av_init_packet(pkt);
            }

            int retval = 0;
            if (!ic || ((retval = av_read_frame(ic, pkt)) < 0))
            {
                if (retval == -EAGAIN)
                    continue;

                SetEof(true);
                delete pkt;
                errno = -retval;
                LOG(VB_GENERAL, LOG_ERR, QString("decoding error") + ENO);
                return false;
            }

            if (waitingForChange && pkt->pos >= readAdjust)
                FileChanged();

            if (pkt->pos > readAdjust)
                pkt->pos -= readAdjust;
        }

        if (!ic)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No context");
            av_free_packet(pkt);
            continue;
        }

        if (pkt->stream_index >= (int)ic->nb_streams)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bad stream");
            av_free_packet(pkt);
            continue;
        }

        AVStream *curstream = ic->streams[pkt->stream_index];

        if (!curstream)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bad stream (NULL)");
            av_free_packet(pkt);
            continue;
        }

        enum AVMediaType codec_type = curstream->codec->codec_type;

        if (storevideoframes && codec_type == AVMEDIA_TYPE_VIDEO)
        {
            av_dup_packet(pkt);
            storedPackets.append(pkt);
            pkt = NULL;
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_VIDEO &&
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

        if (codec_type == AVMEDIA_TYPE_SUBTITLE &&
            curstream->codec->codec_id == AV_CODEC_ID_TEXT)
        {
            ProcessRawTextPacket(pkt);
            av_free_packet(pkt);
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_SUBTITLE &&
            curstream->codec->codec_id == AV_CODEC_ID_DVB_TELETEXT)
        {
            ProcessDVBDataPacket(curstream, pkt);
            av_free_packet(pkt);
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_DATA)
        {
            ProcessDataPacket(curstream, pkt, decodetype);
            av_free_packet(pkt);
            continue;
        }

        if (!curstream->codec->codec)
        {
            if (codec_type != AVMEDIA_TYPE_VIDEO)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("No codec for stream index %1, type(%2) id(%3:%4)")
                        .arg(pkt->stream_index)
                        .arg(ff_codec_type_string(codec_type))
                        .arg(ff_codec_id_string(curstream->codec->codec_id))
                        .arg(curstream->codec->codec_id));
            }
            av_free_packet(pkt);
            continue;
        }

        have_err = false;

        switch (codec_type)
        {
            case AVMEDIA_TYPE_AUDIO:
            {
                if (!ProcessAudioPacket(curstream, pkt, decodetype))
                    have_err = true;
                else
                    GenerateDummyVideoFrames();
                break;
            }

            case AVMEDIA_TYPE_VIDEO:
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
                    gotVideoFrame = 1;
                    break;
                }

                if (!ProcessVideoPacket(curstream, pkt))
                    have_err = true;
                break;
            }

            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (!ProcessSubtitlePacket(curstream, pkt))
                    have_err = true;
                break;
            }

            default:
            {
                AVCodecContext *enc = curstream->codec;
                LOG(VB_GENERAL, LOG_ERR, LOC +
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
    if (ic && ic->cur_pmt_sect)
    {
        const PESPacket pes = PESPacket::ViewData(ic->cur_pmt_sect);
        const PSIPTable psip(pes);
        const ProgramMapTable pmt(psip);

        for (uint i = 0; i < pmt.StreamCount(); i++)
        {
            // MythTV remaps OpenCable Video to normal video during recording
            // so "dvb" is the safest choice for system info type, since this
            // will ignore other uses of the same stream id in DVB countries.
            if (pmt.IsVideo(i, "dvb"))
                return true;

            // MHEG may explicitly select a private stream as video
            if ((i == (uint)selectedTrack[kTrackTypeVideo].av_stream_index) &&
                (pmt.StreamType(i) == StreamID::PrivData))
            {
                return true;
            }
        }
    }

    return GetTrackCount(kTrackTypeVideo);
}

bool AvFormatDecoder::GenerateDummyVideoFrames(void)
{
    while (needDummyVideoFrames && m_parent &&
           m_parent->GetFreeVideoFrames())
    {
        VideoFrame *frame = m_parent->GetNextVideoFrame();
        if (!frame)
            return false;

        m_parent->ClearDummyVideoFrame(frame);
        m_parent->ReleaseNextVideoFrame(frame, lastvpts);
        m_parent->DeLimboFrame(frame);

        frame->interlaced_frame = 0; // not interlaced
        frame->top_field_first  = 1; // top field first
        frame->repeat_pict      = 0; // not a repeated picture
        frame->frameNumber      = framesPlayed;
        frame->dummy            = 1;

        decoded_video_frame = frame;
        framesPlayed++;
        gotVideoFrame = true;
    }
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
        LOG(VB_AUDIO, LOG_INFO, LOC + msg + " pass through");

        // Force pass through state to be reanalyzed
        QMutexLocker locker(avcodeclock);
        SetupAudioStream();
    }
}

inline bool AvFormatDecoder::DecoderWillDownmix(const AVCodecContext *ctx)
{
    // Until ffmpeg properly implements dialnorm
    // use Myth internal downmixer if machines has FPU/SSE
    if (m_audio->CanDownmix() && AudioOutputUtil::has_hardware_fpu())
        return false;
    if (!m_audio->CanDownmix())
        return true;
    // use ffmpeg only for dolby codecs if we have to
    switch (ctx->codec_id)
    {
        case AV_CODEC_ID_AC3:
        case AV_CODEC_ID_TRUEHD:
        case AV_CODEC_ID_EAC3:
            return true;
        default:
            return false;
    }
}

bool AvFormatDecoder::DoPassThrough(const AVCodecContext *ctx, bool withProfile)
{
    bool passthru;

    // if withProfile == false, we will accept any DTS stream regardless
    // of its profile. We do so, so we can bitstream DTS-HD as DTS core
    if (!withProfile && ctx->codec_id == AV_CODEC_ID_DTS && !m_audio->CanDTSHD())
        passthru = m_audio->CanPassthrough(ctx->sample_rate, ctx->channels,
                                           ctx->codec_id, FF_PROFILE_DTS);
    else
        passthru = m_audio->CanPassthrough(ctx->sample_rate, ctx->channels,
                                           ctx->codec_id, ctx->profile);

    passthru &= !disable_passthru;

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
        AVSampleFormat format_pack = av_get_packed_sample_fmt(ctx->sample_fmt);

        if (format_pack != ctx->sample_fmt)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Audio data is planar"));
        }

        switch (format_pack)
        {
            case AV_SAMPLE_FMT_U8:     fmt = FORMAT_U8;    break;
            case AV_SAMPLE_FMT_S16:    fmt = FORMAT_S16;   break;
            case AV_SAMPLE_FMT_FLT:    fmt = FORMAT_FLT;   break;
            case AV_SAMPLE_FMT_DBL:    fmt = FORMAT_NONE;  break;
            case AV_SAMPLE_FMT_S32:
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
            int bps = av_get_bytes_per_sample(ctx->sample_fmt) << 3;
            if (ctx->sample_fmt == AV_SAMPLE_FMT_S32 &&
                ctx->bits_per_raw_sample)
                bps = ctx->bits_per_raw_sample;
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Unsupported sample format with %1 bits").arg(bps));
            return false;
        }

        using_passthru = DoPassThrough(ctx, false);

        ctx->request_channels = ctx->channels;

        if (!using_passthru &&
            ctx->channels > (int)m_audio->GetMaxChannels() &&
            DecoderWillDownmix(ctx))
        {
            ctx->request_channels = m_audio->GetMaxChannels();
        }

        info = AudioInfo(ctx->codec_id, fmt, ctx->sample_rate,
                         ctx->channels, using_passthru, orig_channels,
                         ctx->codec_id == AV_CODEC_ID_DTS ? ctx->profile : 0);
    }

    if (!ctx)
    {
        if (GetTrackCount(kTrackTypeAudio))
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "No codec context. Returning false");
        return false;
    }

    if (info == audioIn)
        return false;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Initializing audio parms from " +
        QString("audio track #%1").arg(currentTrack[kTrackTypeAudio]+1));

    audioOut = audioIn = info;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Audio format changed " +
        QString("\n\t\t\tfrom %1 to %2")
            .arg(old_in.toString()).arg(audioOut.toString()));

    m_audio->SetAudioParams(audioOut.format, orig_channels,
                            ctx->request_channels,
                            audioOut.codec_id, audioOut.sample_rate,
                            audioOut.do_passthru, audioOut.codec_profile);
    m_audio->ReinitAudio();

    if (LCD *lcd = LCD::Get())
    {
        LCDAudioFormatSet audio_format;

        switch (ctx->codec_id)
        {
            case AV_CODEC_ID_MP2:
                audio_format = AUDIO_MPEG2;
                break;
            case AV_CODEC_ID_MP3:
                audio_format = AUDIO_MP3;
                break;
            case AV_CODEC_ID_AC3:
                audio_format = AUDIO_AC3;
                break;
            case AV_CODEC_ID_DTS:
                audio_format = AUDIO_DTS;
                break;
            case AV_CODEC_ID_VORBIS:
                audio_format = AUDIO_OGG;
                break;
            case AV_CODEC_ID_WMAV1:
                audio_format = AUDIO_WMA;
                break;
            case AV_CODEC_ID_WMAV2:
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

void AvFormatDecoder::av_update_stream_timings_video(AVFormatContext *ic)
{
    int64_t start_time, start_time1, end_time, end_time1;
    int64_t duration, duration1, filesize;
    AVStream *st = NULL;

    start_time = INT64_MAX;
    end_time = INT64_MIN;

    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st1 = ic->streams[i];
        if (st1 && st1->codec->codec_type == AVMEDIA_TYPE_VIDEO)
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
           end_time1 = start_time1 +
                      av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
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
        if (ic->pb && (filesize = avio_size(ic->pb)) > 0) {
            /* compute the bitrate */
            ic->bit_rate = (double)filesize * 8.0 * AV_TIME_BASE /
                (double)ic->duration;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
