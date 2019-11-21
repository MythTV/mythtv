// C++ headers
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <unistd.h>
using namespace std;

#include <QTextCodec>
#include <QFileInfo>

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
#include "mythavutil.h"

#include "lcddevice.h"

#include "audiooutput.h"
#include "mythcodeccontext.h"

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

#ifdef USING_MEDIACODEC
#include "mediacodeccontext.h"
extern "C" {
#include "libavcodec/jni.h"
}
#include <QtAndroidExtras>
#endif

#ifdef USING_VAAPI2
#include "vaapi2context.h"
#endif

#ifdef USING_NVDEC
#include "nvdeccontext.h"
#endif

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/error.h"
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavformat/internal.h"
#include "libswscale/swscale.h"
#include "libavformat/isom.h"
#include "ivtv_myth.h"
#include "libavutil/imgutils.h"
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

// Maximum number of sequential invalid data packet errors
// before we try switching to software decoder
#define SEQ_PKT_ERR_MAX 10

static const int max_video_queue_size = 220;

static int cc608_parity(uint8_t byte);
static int cc608_good_parity(const int *parity_table, uint16_t data);
static void cc608_build_parity_table(int *parity_table);

static bool silence_ffmpeg_logging = false;

static QSize get_video_dim(const AVCodecContext &ctx)
{
    return {ctx.width >> ctx.lowres, ctx.height >> ctx.lowres};
}
static float get_aspect(const AVCodecContext &ctx)
{
    float aspect_ratio = 0.0F;

    if (ctx.sample_aspect_ratio.num && ctx.height)
    {
        aspect_ratio = av_q2d(ctx.sample_aspect_ratio) *
            static_cast<double>(ctx.width);
        aspect_ratio /= (float) ctx.height;
    }

    if (aspect_ratio <= 0.0F || aspect_ratio > 6.0F)
    {
        if (ctx.height)
            aspect_ratio = (float)ctx.width / (float)ctx.height;
        else
            aspect_ratio = 4.0F / 3.0F;
    }

    return aspect_ratio;
}
static float get_aspect(H264Parser &p)
{
    static const float default_aspect = 4.0F / 3.0F;
    int asp = p.aspectRatio();
    switch (asp)
    {
        case 0: return default_aspect;
        case 2: return 4.0F / 3.0F;
        case 3: return 16.0F / 9.0F;
        case 4: return 2.21F;
        default: break;
    }

    float aspect_ratio = asp * 0.000001F;
    if (aspect_ratio <= 0.0F || aspect_ratio > 6.0F)
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


int  get_avf_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);
void release_avf_buffer(void *opaque, uint8_t *data);
#ifdef USING_VDPAU
int  get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic, int flags);
void release_avf_buffer_vdpau(void *opaque, uint8_t *data);
int render_wrapper_vdpau(struct AVCodecContext *s, AVFrame *src,
                         const VdpPictureInfo *info,
                         uint32_t count,
                         const VdpBitstreamBuffer *buffers);
#endif
#ifdef USING_DXVA2
int  get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic, int flags);
#endif
#ifdef USING_VAAPI
int  get_avf_buffer_vaapi(struct AVCodecContext *c, AVFrame *pic, int flags);
#endif
#ifdef USING_VAAPI2
int  get_avf_buffer_vaapi2(struct AVCodecContext *c, AVFrame *pic, int flags);
#endif
#ifdef USING_NVDEC
int  get_avf_buffer_nvdec(struct AVCodecContext *c, AVFrame *pic, int flags);
#endif

static int determinable_frame_size(struct AVCodecContext *avctx)
{
    if (/*avctx->codec_id == AV_CODEC_ID_AAC ||*/
        avctx->codec_id == AV_CODEC_ID_MP1 ||
        avctx->codec_id == AV_CODEC_ID_MP2 ||
        avctx->codec_id == AV_CODEC_ID_MP3/* ||
        avctx->codec_id == AV_CODEC_ID_CELT*/)
        return 1;
    return 0;
}

static int has_codec_parameters(AVStream *st)
{
    AVCodecContext *avctx = nullptr;

#define FAIL(errmsg) do {                                     \
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + (errmsg));              \
    return 0;                                                 \
} while (false)

    switch (st->codecpar->codec_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            avctx = gCodecMap->getCodecContext(st);
            if (!avctx)
                FAIL("No codec for audio stream");
            if (!avctx->frame_size && determinable_frame_size(avctx))
                FAIL("unspecified frame size");
            if (avctx->sample_fmt == AV_SAMPLE_FMT_NONE)
                FAIL("unspecified sample format");
            if (!avctx->sample_rate)
                FAIL("unspecified sample rate");
            if (!avctx->channels)
                FAIL("unspecified number of channels");
            if (!st->nb_decoded_frames && avctx->codec_id == AV_CODEC_ID_DTS)
                FAIL("no decodable DTS frames");
            break;
        case AVMEDIA_TYPE_VIDEO:
            avctx = gCodecMap->getCodecContext(st);
            if (!avctx)
                FAIL("No codec for video stream");
            if (!avctx->width)
                FAIL("unspecified size");
            if (avctx->pix_fmt == AV_PIX_FMT_NONE)
                FAIL("unspecified pixel format");
            if (avctx->codec_id == AV_CODEC_ID_RV30 || avctx->codec_id == AV_CODEC_ID_RV40)
                if (!st->sample_aspect_ratio.num && !avctx->sample_aspect_ratio.num && !st->codec_info_nb_frames)
                    FAIL("no frame in rv30/40 and no sar");
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            if (st->codecpar->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE && !st->codecpar->width)
                FAIL("unspecified size");
            break;
        case AVMEDIA_TYPE_DATA:
            if(st->codecpar->codec_id == AV_CODEC_ID_NONE) return 1;
            break;
        default:
            break;
    }

    if (st->codecpar->codec_id == AV_CODEC_ID_NONE)
        FAIL("unknown codec");
    return 1;
}

static bool force_sw_decode(AVCodecContext *avctx)
{
    switch (avctx->codec_id)
    {
        case AV_CODEC_ID_H264:
            switch (avctx->profile)
            {
                case FF_PROFILE_H264_HIGH_10:
                case FF_PROFILE_H264_HIGH_10_INTRA:
                case FF_PROFILE_H264_HIGH_422:
                case FF_PROFILE_H264_HIGH_422_INTRA:
                case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
                case FF_PROFILE_H264_HIGH_444_INTRA:
                case FF_PROFILE_H264_CAVLC_444:
                    return true;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return false;
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
    uint64_t   verbose_mask  = VB_LIBAV;
    LogLevel_t verbose_level = LOG_EMERG;

    // determine mythtv debug level from av log level
    switch (level)
    {
        case AV_LOG_PANIC:
            verbose_level = LOG_EMERG;
            verbose_mask |= VB_GENERAL;
            break;
        case AV_LOG_FATAL:
            verbose_level = LOG_CRIT;
            verbose_mask |= VB_GENERAL;
            break;
        case AV_LOG_ERROR:
            verbose_level = LOG_ERR;
            break;
        case AV_LOG_WARNING:
            verbose_level = LOG_WARNING;
            break;
        case AV_LOG_INFO:
            verbose_level = LOG_INFO;
            break;
        case AV_LOG_VERBOSE:
        case AV_LOG_DEBUG:
        case AV_LOG_TRACE:
            verbose_level = LOG_DEBUG;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
        return;

    string_lock.lock();
    if (full_line.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        full_line = QString("[%1 @ %2] ")
            .arg(avc->item_name(ptr))
            .arg((quintptr)avc, QT_POINTER_SIZE * 2, 16, QChar('0'));
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
    if (lang_cstr[2] == '\0')
    {
        QString tmp2 = lang_cstr;
        QString tmp3 = iso639_str2_to_str3(tmp2);
        int lang = iso639_str3_to_key(tmp3);
        return iso639_key_to_canonical_key(lang);
    }
    int lang = iso639_str3_to_key(lang_cstr);
    return iso639_key_to_canonical_key(lang);
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
#ifdef USING_VAAPI2
    opts.decoders->append("vaapi2");
    (*opts.equiv_decoders)["vaapi2"].append("dummy");
#endif
#ifdef USING_NVDEC
    opts.decoders->append("nvdec");
    (*opts.equiv_decoders)["nvdec"].append("dummy");
#endif
#ifdef USING_MEDIACODEC
    opts.decoders->append("mediacodec");
    (*opts.equiv_decoders)["mediacodec"].append("dummy");
#endif

    PrivateDecoder::GetDecoders(opts);
}

AvFormatDecoder::AvFormatDecoder(MythPlayer *parent,
                                 const ProgramInfo &pginfo,
                                 PlayerFlags flags)
    : DecoderBase(parent, pginfo),
      m_is_db_ignored(gCoreContext->IsDatabaseIgnored()),
      m_h264_parser(new H264Parser()),
      playerFlags(flags),
      // Closed Caption & Teletext decoders
      m_ccd608(new CC608Decoder(parent->GetCC608Reader())),
      m_ccd708(new CC708Decoder(parent->GetCC708Reader())),
      m_ttd(new TeletextDecoder(parent->GetTeletextReader()))
{
    m_audioSamples = (uint8_t *)av_mallocz(AudioOutput::MAX_SIZE_BUFFER);
    m_ccd608->SetIgnoreTimecode(true);

    av_log_set_callback(myth_av_log);

    m_audioIn.sample_size = -32; // force SetupAudioStream to run once
    m_itv = m_parent->GetInteractiveTV();

    cc608_build_parity_table(m_cc608_parity_table);

    AvFormatDecoder::SetIdrOnlyKeyframes(true);
    m_audioReadAhead = gCoreContext->GetNumSetting("AudioReadAhead", 100);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PlayerFlags: 0x%1, AudioReadAhead: %2 msec")
        .arg(playerFlags, 0, 16).arg(m_audioReadAhead));
}

AvFormatDecoder::~AvFormatDecoder()
{
    while (!m_storedPackets.isEmpty())
    {
        AVPacket *pkt = m_storedPackets.takeFirst();
        av_packet_unref(pkt);
        delete pkt;
    }

    CloseContext();
    delete m_ccd608;
    delete m_ccd708;
    delete m_ttd;
    delete m_private_dec;
    delete m_h264_parser;
    delete m_mythcodecctx;

    sws_freeContext(m_sws_ctx);

    av_freep(&m_audioSamples);

    delete m_avfRingBuffer;

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
    if (m_ic)
    {
        for (uint i = 0; i < m_ic->nb_streams; i++)
        {
            QMutexLocker locker(avcodeclock);
            AVStream *st = m_ic->streams[i];
            gCodecMap->freeCodecContext(st);
        }
    }
}

void AvFormatDecoder::CloseContext()
{
    if (m_ic)
    {
        CloseCodecs();

        AVInputFormat *fmt = m_ic->iformat;
        m_ic->iformat->flags |= AVFMT_NOFILE;

        av_free(m_ic->pb->buffer);
        av_free(m_ic->pb);
        avformat_close_input(&m_ic);
        m_ic = nullptr;
        fmt->flags &= ~AVFMT_NOFILE;
    }

    delete m_private_dec;
    m_private_dec = nullptr;
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

    AVStream *st = nullptr;
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *st1 = m_ic->streams[i];
        if (st1 && st1->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }
    if (!st)
        return 0;

    if (m_ic->start_time != AV_NOPTS_VALUE)
        start_pts = av_rescale(m_ic->start_time,
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

    if (m_ic->start_time != AV_NOPTS_VALUE)
        start_pts = av_rescale(m_ic->start_time,
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
    if (m_ic && m_ic->nb_chapters > 1)
        return m_ic->nb_chapters;
    return 0;
}

void AvFormatDecoder::GetChapterTimes(QList<long long> &times)
{
    int total = GetNumChapters();
    if (!total)
        return;

    for (int i = 0; i < total; i++)
    {
        int num = m_ic->chapters[i]->time_base.num;
        int den = m_ic->chapters[i]->time_base.den;
        int64_t start = m_ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
        times.push_back((long long)total_secs);
    }
}

int AvFormatDecoder::GetCurrentChapter(long long framesPlayed)
{
    if (!GetNumChapters())
        return 0;

    for (int i = (m_ic->nb_chapters - 1); i > -1 ; i--)
    {
        int num = m_ic->chapters[i]->time_base.num;
        int den = m_ic->chapters[i]->time_base.den;
        int64_t start = m_ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
        long long framenum = (long long)(total_secs * m_fps);
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

    int num = m_ic->chapters[chapter - 1]->time_base.num;
    int den = m_ic->chapters[chapter - 1]->time_base.den;
    int64_t start = m_ic->chapters[chapter - 1]->start;
    long double total_secs = (long double)start * (long double)num /
                             (long double)den;
    long long framenum = (long long)(total_secs * m_fps);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("GetChapter %1: framenum %2")
                                   .arg(chapter).arg(framenum));
    return framenum;
}

bool AvFormatDecoder::DoRewind(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("DoRewind(%1, %2 discard frames)")
            .arg(desiredFrame).arg( discardFrames ? "do" : "don't" ));

    if (m_recordingHasPositionMap || m_livetv)
        return DecoderBase::DoRewind(desiredFrame, discardFrames);

    m_dorewind = true;

    // avformat-based seeking
    return DoFastForward(desiredFrame, discardFrames);
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DoFastForward(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(m_framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (m_recordingHasPositionMap || m_livetv)
        return DecoderBase::DoFastForward(desiredFrame, discardFrames);

    bool oldrawstate = m_getrawframes;
    m_getrawframes = false;

    AVStream *st = nullptr;
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *st1 = m_ic->streams[i];
        if (st1 && st1->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }

    int seekDelta = desiredFrame - m_framesPlayed;

    // avoid using av_frame_seek if we are seeking frame-by-frame when paused
    if (seekDelta >= 0 && seekDelta < 2 && !m_dorewind && m_parent->GetPlaySpeed() == 0.0F)
    {
        SeekReset(m_framesPlayed, seekDelta, false, true);
        m_parent->SetFramesPlayed(m_framesPlayed + 1);
        m_getrawframes = oldrawstate;
        return true;
    }

    long long ts = 0;
    if (m_ic->start_time != AV_NOPTS_VALUE)
        ts = m_ic->start_time;

    // convert framenumber to normalized timestamp
    long double seekts = desiredFrame * AV_TIME_BASE / m_fps;
    ts += (long long)seekts;

    // XXX figure out how to do snapping in this case
    bool exactseeks = DecoderBase::GetSeekSnap() == 0U;

    int flags = (m_dorewind || exactseeks) ? AVSEEK_FLAG_BACKWARD : 0;

    if (av_seek_frame(m_ic, -1, ts, flags) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_seek_frame(ic, -1, %1, 0) -- error").arg(ts));
        m_getrawframes = oldrawstate;
        return false;
    }

    int normalframes = 0;

    if (st && st->cur_dts != AV_NOPTS_VALUE)
    {

        int64_t adj_cur_dts = st->cur_dts;

        if (m_ic->start_time != AV_NOPTS_VALUE)
        {
            int64_t st1 = av_rescale(m_ic->start_time,
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

        m_lastKey = (long long)((newts*(long double)m_fps)/AV_TIME_BASE);
        m_framesPlayed = m_lastKey;
        m_fpsSkip = 0;
        m_framesRead = m_lastKey;

        normalframes = (exactseeks) ? desiredFrame - m_framesPlayed : 0;
        normalframes = max(normalframes, 0);
        m_no_dts_hack = false;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No DTS Seeking Hack!");
        m_no_dts_hack = true;
        m_framesPlayed = desiredFrame;
        m_fpsSkip = 0;
        m_framesRead = desiredFrame;
        normalframes = 0;
    }

    SeekReset(m_lastKey, normalframes, true, discardFrames);

    if (discardFrames)
        m_parent->SetFramesPlayed(m_framesPlayed + 1);

    m_dorewind = false;

    m_getrawframes = oldrawstate;

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

    QMutexLocker locker(avcodeclock);

    if (doflush)
    {
        m_lastapts = 0;
        m_lastvpts = 0;
        m_lastccptsu = 0;
        m_faulty_pts = m_faulty_dts = 0;
        m_last_pts_for_fault_detection = 0;
        m_last_dts_for_fault_detection = 0;
        m_pts_detected = false;
        m_reordered_pts_detected = false;

        ff_read_frame_flush(m_ic);

        // Only reset the internal state if we're using our seeking,
        // not when using libavformat's seeking
        if (m_recordingHasPositionMap || m_livetv)
        {
            m_ic->pb->pos = ringBuffer->GetReadPosition();
            m_ic->pb->buf_ptr = m_ic->pb->buffer;
            m_ic->pb->buf_end = m_ic->pb->buffer;
            m_ic->pb->eof_reached = 0;
        }

        // Flush the avcodec buffers
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "SeekReset() flushing");
        for (uint i = 0; i < m_ic->nb_streams; i++)
        {
            AVCodecContext *enc = gCodecMap->hasCodecContext(m_ic->streams[i]);
            // note that contexts that have not been opened have
            // enc->internal = nullptr and cause a segfault in
            // avcodec_flush_buffers
            if (enc && enc->internal)
                avcodec_flush_buffers(enc);
        }
        if (m_private_dec)
            m_private_dec->Reset();
    }

    // Discard all the queued up decoded frames
    if (discardFrames)
        m_parent->DiscardVideoFrames(doflush);

    if (doflush)
    {
        // Free up the stored up packets
        while (!m_storedPackets.isEmpty())
        {
            AVPacket *pkt = m_storedPackets.takeFirst();
            av_packet_unref(pkt);
            delete pkt;
        }

        m_prevgoppos = 0;
        m_gopset = false;
        if (!ringBuffer->IsDVD())
        {
            if (!m_no_dts_hack)
            {
                m_framesPlayed = m_lastKey;
                m_fpsSkip = 0;
                m_framesRead = m_lastKey;
            }

            m_no_dts_hack = false;
        }
    }

    // Skip all the desired number of skipFrames

    // Some seeks can be very slow.  The most common example comes
    // from HD-PVR recordings, where keyframes are 128 frames apart
    // and decoding (even hardware decoding) may not be much faster
    // than realtime, causing some exact seeks to take 2-4 seconds.
    // If exact seeking is not required, we take some shortcuts.
    // First, we impose an absolute maximum time we are willing to
    // spend (maxSeekTimeMs) on the forward frame-by-frame skip.
    // After that much time has elapsed, we give up and stop the
    // frame-by-frame seeking.  Second, after skipping a few frames,
    // we predict whether the situation is hopeless, i.e. the total
    // skipping would take longer than giveUpPredictionMs, and if so,
    // stop skipping right away.
    bool exactSeeks = GetSeekSnap() == 0U;
    const int maxSeekTimeMs = 200;
    int profileFrames = 0;
    MythTimer begin(MythTimer::kStartRunning);
    for (; (skipFrames > 0 && !m_ateof &&
            (exactSeeks || begin.elapsed() < maxSeekTimeMs));
         --skipFrames, ++profileFrames)
    {
        GetFrame(kDecodeVideo);
        if (m_decoded_video_frame)
        {
            m_parent->DiscardVideoFrame(m_decoded_video_frame);
            m_decoded_video_frame = nullptr;
        }
        if (!exactSeeks && profileFrames >= 5 && profileFrames < 10)
        {
            const int giveUpPredictionMs = 400;
            int remainingTimeMs =
                skipFrames * (float)begin.elapsed() / profileFrames;
            if (remainingTimeMs > giveUpPredictionMs)
            {
              LOG(VB_PLAYBACK, LOG_DEBUG,
                  QString("Frame-by-frame seeking would take "
                          "%1 ms to finish, skipping.").arg(remainingTimeMs));
              break;
            }
        }
    }

    if (doflush)
    {
        m_firstvpts = 0;
        m_firstvptsinuse = true;
    }
}

void AvFormatDecoder::SetEof(bool eof)
{
    if (!eof && m_ic && m_ic->pb)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("Resetting byte context eof (livetv %1 was eof %2)")
                .arg(m_livetv).arg(m_ic->pb->eof_reached));
        m_ic->pb->eof_reached = 0;
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
        m_seen_gop = false;
        m_seq_count = 0;
    }
}

bool AvFormatDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize],
                                const QString &filename, int testbufsize)
{
    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));

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

    memset(probe.buf + probe.buf_size, 0, AVPROBE_PADDING_SIZE);

    return av_probe_input_format2(&probe, static_cast<int>(true), &score) != nullptr;
}

void AvFormatDecoder::InitByteContext(bool forceseek)
{
    int buf_size                = ringBuffer->BestBufferSize();
    int streamed                = ringBuffer->IsStreamed();
    m_readcontext.prot            = AVFRingBuffer::GetRingBufferURLProtocol();
    m_readcontext.flags           = AVIO_FLAG_READ;
    m_readcontext.is_streamed     = streamed;
    m_readcontext.max_packet_size = 0;
    m_readcontext.priv_data       = m_avfRingBuffer;
    unsigned char* buffer       = (unsigned char *)av_malloc(buf_size);
    m_ic->pb                      = avio_alloc_context(buffer, buf_size, 0,
                                                      &m_readcontext,
                                                      AVFRingBuffer::AVF_Read_Packet,
                                                      AVFRingBuffer::AVF_Write_Packet,
                                                      AVFRingBuffer::AVF_Seek_Packet);

    // We can always seek during LiveTV
    m_ic->pb->seekable            = !streamed || forceseek;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Buffer size: %1 streamed %2 seekable %3")
        .arg(buf_size).arg(streamed).arg(m_ic->pb->seekable));
}

extern "C" void HandleStreamChange(void *data)
{
    AvFormatDecoder *decoder =
        reinterpret_cast<AvFormatDecoder*>(data);

    int cnt = decoder->m_ic->nb_streams;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("streams_changed 0x%1 -- stream count %2")
            .arg((uint64_t)data,0,16).arg(cnt));

    decoder->m_streams_changed = true;
}

int AvFormatDecoder::FindStreamInfo(void)
{
    QMutexLocker lock(avcodeclock);
    int retval = avformat_find_stream_info(m_ic, nullptr);
    silence_ffmpeg_logging = false;
    // ffmpeg 3.0 is returning -1 code when there is a channel
    // change or some encoding error just after the start
    // of the file, but is has found the correct stream info
    // Set rc to 0 so that playing can continue.
    if (retval == -1)
        retval = 0;
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
 *  \param testbuf Temporary buffer for probing the input.
 *  \param testbufsize The size of the test buffer. The minimum of this value
 *                     or kDecoderProbeBufferSize will be used.
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

    delete m_avfRingBuffer;
    m_avfRingBuffer = new AVFRingBuffer(rbuffer);

    AVInputFormat *fmt      = nullptr;
    QString        fnames   = ringBuffer->GetFilename();
    QByteArray     fnamea   = fnames.toLatin1();
    const char    *filename = fnamea.constData();

    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));
    probe.filename = filename;
    probe.buf = (unsigned char *)testbuf;
    if (testbufsize + AVPROBE_PADDING_SIZE <= kDecoderProbeBufferSize)
        probe.buf_size = testbufsize;
    else
        probe.buf_size = kDecoderProbeBufferSize - AVPROBE_PADDING_SIZE;

    memset(probe.buf + probe.buf_size, 0, AVPROBE_PADDING_SIZE);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "OpenFile -- begin");

    fmt = av_probe_input_format(&probe, static_cast<int>(true));
    if (!fmt)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Probe failed for file: \"%1\".").arg(filename));
        return -1;
    }

    if (strcmp(fmt->name, "mpegts") == 0 &&
        gCoreContext->GetBoolSetting("FFMPEGTS", false))
    {
        AVInputFormat *fmt2 = av_find_input_format("mpegts-ffmpeg");
        if (fmt2)
        {
            fmt = fmt2;
            LOG(VB_GENERAL, LOG_INFO, LOC + "Using FFmpeg MPEG-TS demuxer (forced)");
        }
    }

    int err = 0;
    bool found = false;
    bool scanned = false;

    if (m_livetv)
    {
        // We try to open the file for up to 1.5 second using only buffer in memory
        MythTimer timer; timer.start();

        m_avfRingBuffer->SetInInit(true);

        while (!found && timer.elapsed() < 1500)
        {
            m_ic = avformat_alloc_context();
            if (!m_ic)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Could not allocate format context.");
                return -1;
            }

            InitByteContext(true);

            err = avformat_open_input(&m_ic, filename, fmt, nullptr);
            if (err < 0)
            {
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("avformat_open_input failed for in ram data after %1ms, retrying in 50ms")
                    .arg(timer.elapsed()));
                usleep(50 * 1000);  // wait 50ms
                continue;
            }

            // Test if we can find all streams details in what has been found so far
            if (FindStreamInfo() < 0)
            {
                avformat_close_input(&m_ic);
                LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                    QString("FindStreamInfo failed for in ram data after %1ms, retrying in 50ms")
                    .arg(timer.elapsed()));
                usleep(50 * 1000);  // wait 50ms
                continue;
            }

            found = true;

            for (uint i = 0; i < m_ic->nb_streams; i++)
            {
                if (!has_codec_parameters(m_ic->streams[i]))
                {
                    avformat_close_input(&m_ic);
                    found = false;
                    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
                        QString("Invalid streams found in ram data after %1ms, retrying in 50ms")
                        .arg(timer.elapsed()));
                    usleep(50 * 1000);  // wait 50ms
                    break;
                }
            }
        }

        m_avfRingBuffer->SetInInit(false);

        if (found)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("File successfully opened after %1ms")
                .arg(timer.elapsed()));
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_WARNING, LOC +
                QString("No streams found in ram data after %1ms, defaulting to in-file")
                .arg(timer.elapsed()));
        }
        scanned = found;
    }

    // If we haven't opened the file so far, revert to old method
    while (!found)
    {
        m_ic = avformat_alloc_context();
        if (!m_ic)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not allocate format context.");
            return -1;
        }

        InitByteContext();

        err = avformat_open_input(&m_ic, filename, fmt, nullptr);
        if (err < 0)
        {
            if (strcmp(fmt->name, "mpegts") == 0)
            {
                fmt = av_find_input_format("mpegts-ffmpeg");
                if (fmt)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("avformat_open_input failed with '%1' error.").arg(err));
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Attempting using original FFmpeg MPEG-TS demuxer."));
                    // ic would have been freed due to the earlier failure
                    continue;
                }
                break;
            }
        }
        found = true;
    }
    if (err < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("avformat err(%1) on avformat_open_input call.").arg(err));
        m_ic = nullptr;
        return -1;
    }

    if (!scanned)
    {
        int ret = FindStreamInfo();
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not find codec parameters. " +
                    QString("file was \"%1\".").arg(filename));
            avformat_close_input(&m_ic);
            m_ic = nullptr;
            return -1;
        }
    }

    m_ic->streams_changed = HandleStreamChange;
    m_ic->stream_change_data = this;

    fmt->flags &= ~AVFMT_NOFILE;

    if (!m_livetv && !ringBuffer->IsDisc())
    {
        // generate timings based on the video stream to avoid bogus ffmpeg
        // values for duration and bitrate
        av_update_stream_timings_video(m_ic);
    }

    // FLAC, MP3 or M4A file may contains an artwork image, a single frame MJPEG,
    // we need to ignore it as we don't handle single frames or images in place of video
    // TODO: display single frame
    QString extension = QFileInfo(fnames).suffix();
    if (strcmp(fmt->name, "mp3") == 0 || strcmp(fmt->name, "flac") == 0 ||
        strcmp(fmt->name, "ogg") == 0 ||
        (extension.compare("m4a", Qt::CaseInsensitive) == 0))
    {
        novideo = true;
    }

    // Scan for the initial A/V streams
    int ret = ScanStreams(novideo);
    if (-1 == ret)
        return ret;

    AutoSelectTracks(); // This is needed for transcoder

#ifdef USING_MHEG
    {
        int initialAudio = -1, initialVideo = -1;
        if (m_itv || (m_itv = m_parent->GetInteractiveTV()))
            m_itv->GetInitialStreams(initialAudio, initialVideo);
        if (initialAudio >= 0)
            SetAudioByComponentTag(initialAudio);
        if (initialVideo >= 0)
            SetVideoByComponentTag(initialVideo);
    }
#endif // USING_MHEG

    // Try to get a position map from the recorder if we don't have one yet.
    if (!m_recordingHasPositionMap && !m_is_db_ignored)
    {
        if ((m_playbackinfo) || m_livetv || m_watchingrecording)
        {
            m_recordingHasPositionMap |= SyncPositionMap();
            if (m_recordingHasPositionMap && !m_livetv && !m_watchingrecording)
            {
                m_hasFullPositionMap = true;
                m_gopset = true;
            }
        }
    }

    // If watching pre-recorded television or video use the marked duration
    // from the db if it exists, else ffmpeg duration
    int64_t dur = 0;

    if (m_playbackinfo)
    {
        dur = m_playbackinfo->QueryTotalDuration();
        dur /= 1000;
    }

    if (dur == 0)
    {
        if ((m_ic->duration == AV_NOPTS_VALUE) &&
            (!m_livetv && !ringBuffer->IsDisc()))
            av_estimate_timings(m_ic, 0);

        dur = m_ic->duration / (int64_t)AV_TIME_BASE;
    }

    if (dur > 0 && !m_livetv && !m_watchingrecording)
    {
        m_parent->SetDuration((int)dur);
    }

    // If we don't have a position map, set up ffmpeg for seeking
    if (!m_recordingHasPositionMap && !m_livetv)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Recording has no position -- using libavformat seeking.");

        if (dur > 0)
        {
            m_parent->SetFileLength((int)(dur), (int)(dur * m_fps));
        }
        else
        {
            // the pvr-250 seems to over report the bitrate by * 2
            float bytespersec = (float)m_bitrate / 8 / 2;
            float secs = ringBuffer->GetRealFileSize() * 1.0F / bytespersec;
            m_parent->SetFileLength((int)(secs),
                                    (int)(secs * static_cast<float>(m_fps)));
        }

        // we will not see a position map from db or remote encoder,
        // set the gop interval to 15 frames.  if we guess wrong, the
        // auto detection will change it.
        m_keyframedist = 15;
        m_positionMapType = MARK_GOP_BYFRAME;

        if (strcmp(fmt->name, "avi") == 0)
        {
            // avi keyframes are too irregular
            m_keyframedist = 1;
        }

        m_dontSyncPositionMap = true;
        m_ic->build_index = 1;
    }
    // we have a position map, disable libavformat's seek index
    else
        m_ic->build_index = 0;

    av_dump_format(m_ic, 0, filename, 0);

    // print some useful information if playback debugging is on
    if (m_hasFullPositionMap)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Position map found");
    else if (m_recordingHasPositionMap)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Partial position map found");
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Successfully opened decoder for file: \"%1\". novideo(%2)")
            .arg(filename).arg(novideo));

    // Print AVChapter information
    for (unsigned int i=0; i < m_ic->nb_chapters; i++)
    {
        int num = m_ic->chapters[i]->time_base.num;
        int den = m_ic->chapters[i]->time_base.den;
        int64_t start = m_ic->chapters[i]->start;
        long double total_secs = (long double)start * (long double)num /
                                 (long double)den;
        int hours = (int)total_secs / 60 / 60;
        int minutes = ((int)total_secs / 60) - (hours * 60);
        double secs = (double)total_secs -
                      (double)(hours * 60 * 60 + minutes * 60);
        long long framenum = (long long)(total_secs * m_fps);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Chapter %1 found @ [%2:%3:%4]->%5")
                .arg(QString().sprintf("%02d", i + 1))
                .arg(QString().sprintf("%02d", hours))
                .arg(QString().sprintf("%02d", minutes))
                .arg(QString().sprintf("%06.3f", secs))
                .arg(framenum));
    }

    if (getenv("FORCE_DTS_TIMESTAMPS"))
        m_force_dts_timestamps = true;

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
    return m_recordingHasPositionMap;
}

float AvFormatDecoder::normalized_fps(AVStream *stream, AVCodecContext *enc)
{
    float fps, avg_fps, codec_fps, container_fps, estimated_fps;
    avg_fps = codec_fps = container_fps = estimated_fps = 0.0F;

    if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
        avg_fps = av_q2d(stream->avg_frame_rate); // MKV default_duration

    if (enc->time_base.den && enc->time_base.num) // tbc
        codec_fps = 1.0 / av_q2d(enc->time_base) / enc->ticks_per_frame;
    // Some formats report fps waaay too high. (wrong time_base)
    if (codec_fps > 121.0F && (enc->time_base.den > 10000) &&
        (enc->time_base.num == 1))
    {
        enc->time_base.num = 1001;  // seems pretty standard
        if (av_q2d(enc->time_base) > 0)
            codec_fps = 1.0 / av_q2d(enc->time_base);
    }
    if (stream->time_base.den && stream->time_base.num) // tbn
        container_fps = 1.0 / av_q2d(stream->time_base);
    if (stream->r_frame_rate.den && stream->r_frame_rate.num) // tbr
        estimated_fps = av_q2d(stream->r_frame_rate);

    // matroska demuxer sets the default_duration to avg_frame_rate
    // mov,mp4,m4a,3gp,3g2,mj2 demuxer sets avg_frame_rate
    if ((QString(m_ic->iformat->name).contains("matroska") ||
        QString(m_ic->iformat->name).contains("mov")) &&
        avg_fps < 121.0F && avg_fps > 3.0F)
        fps = avg_fps; // NOLINT(bugprone-branch-clone)
    else if (QString(m_ic->iformat->name).contains("avi") &&
        container_fps < 121.0F && container_fps > 3.0F)
        fps = container_fps; // avi uses container fps for timestamps // NOLINT(bugprone-branch-clone)
    else if (codec_fps < 121.0F && codec_fps > 3.0F)
        fps = codec_fps;
    else if (container_fps < 121.0F && container_fps > 3.0F)
        fps = container_fps;
    else if (estimated_fps < 121.0F && estimated_fps > 3.0F)
        fps = estimated_fps;
    else if (avg_fps < 121.0F && avg_fps > 3.0F)
        fps = avg_fps;
    else
        fps = 30000.0F / 1001.0F; // 29.97 fps

    if (fps != m_fps)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Selected FPS is %1 (avg %2 codec %3 "
                    "container %4 estimated %5)").arg(fps).arg(avg_fps)
                .arg(codec_fps).arg(container_fps).arg(estimated_fps));
    }

    return fps;
}

#ifdef USING_VDPAU
static enum AVPixelFormat get_format_vdpau(struct AVCodecContext *avctx,
                                           const enum AVPixelFormat *valid_fmts)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(avctx->opaque);
    MythPlayer *player = nullptr;
    VideoOutputVDPAU *videoOut = nullptr;
    if (nd)
        player =  nd->GetPlayer();
    if (player)
        videoOut = dynamic_cast<VideoOutputVDPAU*>(player->GetVideoOutput());

    if (videoOut)
    {
        static uint8_t *dummy[1] = { nullptr };
        avctx->hwaccel_context = player->GetDecoderContext(nullptr, dummy[0]);
        MythRenderVDPAU *render = videoOut->getRender();
        render->BindContext(avctx);
        if (avctx->hwaccel_context)
        {
            auto vdpau_context = (AVVDPAUContext*)(avctx->hwaccel_context);
            if (vdpau_context != nullptr)
                vdpau_context->render2 = render_wrapper_vdpau;
        }
    }

    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_VDPAU))
            return AV_PIX_FMT_VDPAU;
        if (not avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_YUV420P))
            return AV_PIX_FMT_YUV420P;
        valid_fmts++;
    }
    return AV_PIX_FMT_NONE;
}
#endif

#ifdef USING_DXVA2
// Declared separately to allow attribute
static enum AVPixelFormat get_format_dxva2(struct AVCodecContext *,
                                           const enum AVPixelFormat *);

enum AVPixelFormat get_format_dxva2(struct AVCodecContext *avctx,
                                    const enum AVPixelFormat *valid_fmts)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(avctx->opaque);
    if (nd && nd->GetPlayer())
    {
        static uint8_t *dummy[1] = { nullptr };
        avctx->hwaccel_context =
            (dxva_context*)nd->GetPlayer()->GetDecoderContext(nullptr, dummy[0]);
    }

    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_DXVA2_VLD))
            return AV_PIX_FMT_DXVA2_VLD;
        if (not avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_YUV420P))
            return AV_PIX_FMT_YUV420P;
        valid_fmts++;
    }
    return AV_PIX_FMT_NONE;
}
#endif

#if defined(USING_VAAPI2) || defined(USING_NVDEC)
static bool IS_VAAPI_PIX_FMT(enum AVPixelFormat fmt)
{
    return fmt == AV_PIX_FMT_VAAPI_MOCO ||
           fmt == AV_PIX_FMT_VAAPI_IDCT ||
           fmt == AV_PIX_FMT_VAAPI_VLD;
}
#endif

#ifdef USING_VAAPI
// Declared separately to allow attribute
static enum AVPixelFormat get_format_vaapi(struct AVCodecContext * /*avctx*/,
                                         const enum AVPixelFormat * /*valid_fmts*/);

enum AVPixelFormat get_format_vaapi(struct AVCodecContext *avctx,
                                         const enum AVPixelFormat *valid_fmts)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(avctx->opaque);
    if (nd && nd->GetPlayer())
    {
        static uint8_t *dummy[1] = { nullptr };
        avctx->hwaccel_context =
            (vaapi_context*)nd->GetPlayer()->GetDecoderContext(nullptr, dummy[0]);
    }

    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_VAAPI_VLD))
            return AV_PIX_FMT_VAAPI_VLD;
        if (not avctx->hwaccel_context and (*valid_fmts == AV_PIX_FMT_YUV420P))
            return AV_PIX_FMT_YUV420P;
        valid_fmts++;
    }
    return AV_PIX_FMT_NONE;
}
#endif

#ifdef USING_VAAPI2
static enum AVPixelFormat get_format_vaapi2(struct AVCodecContext */*avctx*/,
                                           const enum AVPixelFormat *valid_fmts)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (IS_VAAPI_PIX_FMT(*valid_fmts))
        {
            ret = *valid_fmts;
            break;
        }
        valid_fmts++;
    }
    return ret;
}
#endif

#ifdef USING_NVDEC
static enum AVPixelFormat get_format_nvdec(struct AVCodecContext */*avctx*/,
                                           const enum AVPixelFormat *valid_fmts)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (AV_PIX_FMT_CUDA ==  *valid_fmts)
        {
            ret = *valid_fmts;
            break;
        }
        valid_fmts++;
    }
    return ret;
}
#endif

#ifdef USING_MEDIACODEC
static enum AVPixelFormat get_format_mediacodec(struct AVCodecContext */*avctx*/,
                                           const enum AVPixelFormat *valid_fmts)
{
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;
    while (*valid_fmts != AV_PIX_FMT_NONE) {
        if (*valid_fmts == AV_PIX_FMT_MEDIACODEC)
        {
            ret = AV_PIX_FMT_MEDIACODEC;
            break;
        }
        valid_fmts++;
    }
    return ret;
}
#endif


static bool IS_DR1_PIX_FMT(const enum AVPixelFormat fmt)
{
    switch (fmt)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
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
        m_directrendering = false;

    enc->opaque = (void *)this;
    enc->get_buffer2 = get_avf_buffer;
    enc->slice_flags = 0;

    enc->err_recognition = AV_EF_COMPLIANT;
    enc->workaround_bugs = FF_BUG_AUTODETECT;
    enc->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    enc->idct_algo = FF_IDCT_AUTO;
    enc->debug = 0;
    // enc->error_rate = 0;

    const AVCodec *codec1 = enc->codec;

    if (selectedStream)
    {
        m_directrendering = true;
    }

    AVDictionaryEntry *metatag =
        av_dict_get(stream->metadata, "rotate", nullptr, 0);
    if (metatag && metatag->value && QString("180") == metatag->value)
        m_video_inverted = true;

#ifdef USING_VDPAU
    if (codec_is_vdpau(m_video_codec_id))
    {
        enc->get_buffer2     = get_avf_buffer_vdpau;
        enc->get_format      = get_format_vdpau;
        enc->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else
#endif
#ifdef USING_DXVA2
    if (CODEC_IS_DXVA2(codec1, enc))
    {
        enc->get_buffer2     = get_avf_buffer_dxva2;
        enc->get_format      = get_format_dxva2;
    }
    else
#endif
#ifdef USING_VAAPI
    if (CODEC_IS_VAAPI(codec1, enc) && codec_is_vaapi(m_video_codec_id))
    {
        enc->get_buffer2     = get_avf_buffer_vaapi;
        enc->get_format      = get_format_vaapi;
        enc->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else
#endif
#ifdef USING_MEDIACODEC
    if (CODEC_IS_MEDIACODEC(codec1))
    {
        enc->get_format      = get_format_mediacodec;
        enc->slice_flags     = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }
    else
#endif
#ifdef USING_VAAPI2
    if (codec_is_vaapi2(m_video_codec_id))
    {
        enc->get_buffer2     = get_avf_buffer_vaapi2;
        enc->get_format      = get_format_vaapi2;
    }
    else
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec(m_video_codec_id))
    {
        enc->get_buffer2     = get_avf_buffer_nvdec;
        enc->get_format      = get_format_nvdec;
        m_directrendering = false;
    }
    else
#endif
    if (codec1 && codec1->capabilities & AV_CODEC_CAP_DR1)
    {
        // enc->flags          |= CODEC_FLAG_EMU_EDGE;
    }
    else
    {
        if (selectedStream)
            m_directrendering = false;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Using software scaling to convert pixel format %1 for "
                    "codec %2").arg(enc->pix_fmt)
                .arg(ff_codec_id_string(enc->codec_id)));
    }

    // NOLINTNEXTLINE(readability-misleading-indentation)
    QString deinterlacer;
    if (m_mythcodecctx)
        deinterlacer = m_mythcodecctx->getDeinterlacerName();
    delete m_mythcodecctx;
    m_mythcodecctx = MythCodecContext::createMythCodecContext(m_video_codec_id);
    m_mythcodecctx->setPlayer(m_parent);
    m_mythcodecctx->setStream(stream);
    m_mythcodecctx->setDeinterlacer(true,deinterlacer);

    int ret = m_mythcodecctx->HwDecoderInit(enc);
    if (ret < 0)
    {
        if (ret < 0)
        {
            char error[AV_ERROR_MAX_STRING_SIZE];
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("HwDecoderInit unable to initialize hardware decoder: %1 (%2)")
                .arg(av_make_error_string(error, sizeof(error), ret))
                .arg(ret));
            // force it to switch to software decoding
            m_averror_count = SEQ_PKT_ERR_MAX + 1;
            m_streams_changed = true;
        }
    }

    if (FlagIsSet(kDecodeLowRes)    || FlagIsSet(kDecodeSingleThreaded) ||
        FlagIsSet(kDecodeFewBlocks) || FlagIsSet(kDecodeNoLoopFilter)   ||
        FlagIsSet(kDecodeNoDecode))
    {
        if (codec1 &&
            ((AV_CODEC_ID_MPEG2VIDEO == codec1->id) ||
            (AV_CODEC_ID_MPEG1VIDEO == codec1->id)))
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
        else if (codec1 && (AV_CODEC_ID_H264 == codec1->id))
        {
            if (FlagIsSet(kDecodeNoLoopFilter))
            {
                enc->flags &= ~AV_CODEC_FLAG_LOOP_FILTER;
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
        m_fps = normalized_fps(stream, enc);
        QSize dim    = get_video_dim(*enc);
        int   width  = m_current_width  = dim.width();
        int   height = m_current_height = dim.height();
        m_current_aspect = get_aspect(*enc);

        if (!width || !height)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "InitVideoCodec invalid dimensions, resetting decoder.");
            width  = 640;
            height = 480;
            m_fps    = 29.97F;
            m_current_aspect = 4.0F / 3.0F;
        }

        m_parent->SetKeyframeDistance(m_keyframedist);
        const AVCodec *codec2 = enc->codec;
        QString codecName;
        if (codec2)
            codecName = codec2->name;
        m_parent->SetVideoParams(width, height, m_fps, kScan_Detect, codecName);
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
    for (uint8_t byte = 0; byte <= 127; byte++)
    {
        int parity_v = cc608_parity(byte);
        /* CC uses odd parity (i.e., # of 1's in byte is odd.) */
        parity_table[byte] = parity_v;
        parity_table[byte | 0x80] = (parity_v == 0 ? 1 : 0);
    }
}

// CC Parity checking
// taken from xine-lib libspucc

static int cc608_good_parity(const int *parity_table, uint16_t data)
{
    bool ret = (parity_table[data & 0xff] != 0)
        && (parity_table[(data & 0xff00) >> 8] != 0);
    if (!ret)
    {
        LOG(VB_VBI, LOG_ERR, LOC +
            QString("VBI: Bad parity in EIA-608 data (%1)") .arg(data,0,16));
    }
    return ret ? 1 : 0;
}

void AvFormatDecoder::ScanATSCCaptionStreams(int av_index)
{
    memset(m_ccX08_in_pmt, 0, sizeof(m_ccX08_in_pmt));
    m_pmt_tracks.clear();
    m_pmt_track_types.clear();

    // Figure out languages of ATSC captions
    if (!m_ic->cur_pmt_sect)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "ScanATSCCaptionStreams() called with no PMT");
        return;
    }

    const ProgramMapTable pmt(PSIPTable(m_ic->cur_pmt_sect));

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

    for (size_t j = 0; j < desc_list.size(); j++)
    {
        const CaptionServiceDescriptor csd(desc_list[j]);
        if (!csd.IsValid())
            continue;
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
                m_ccX08_in_pmt[key] = true;
                m_pmt_tracks.push_back(si);
                m_pmt_track_types.push_back(kTrackTypeCC708);
            }
            else
            {
                int line21 = csd.Line21Field(k) ? 3 : 1;
                StreamInfo si(av_index, lang, 0/*lang_idx*/, line21, 0);
                m_ccX08_in_pmt[line21-1] = true;
                m_pmt_tracks.push_back(si);
                m_pmt_track_types.push_back(kTrackTypeCC608);
            }
        }
    }
}

void AvFormatDecoder::UpdateATSCCaptionTracks(void)
{
    m_tracks[kTrackTypeCC608].clear();
    m_tracks[kTrackTypeCC708].clear();
    memset(m_ccX08_in_tracks, 0, sizeof(m_ccX08_in_tracks));

    uint pidx = 0, sidx = 0;
    map<int,uint> lang_cc_cnt[2];
    while (true)
    {
        bool pofr = pidx >= (uint)m_pmt_tracks.size();
        bool sofr = sidx >= (uint)m_stream_tracks.size();
        if (pofr && sofr)
            break;

        // choose lowest available next..
        // stream_id's of 608 and 708 streams alias, but this
        // is ok as we just want each list to be ordered.
        StreamInfo const *si = nullptr;
        int type = 0; // 0 if 608, 1 if 708
        bool isp = true; // if true use m_pmt_tracks next, else stream_tracks

        if (pofr && !sofr)
            isp = false; // NOLINT(bugprone-branch-clone)
        else if (!pofr && sofr)
            isp = true;
        else if (m_stream_tracks[sidx] < m_pmt_tracks[pidx])
            isp = false;

        if (isp)
        {
            si = &m_pmt_tracks[pidx];
            type = kTrackTypeCC708 == m_pmt_track_types[pidx] ? 1 : 0;
            pidx++;
        }
        else
        {
            si = &m_stream_tracks[sidx];
            type = kTrackTypeCC708 == m_stream_track_types[sidx] ? 1 : 0;
            sidx++;
        }

        StreamInfo nsi(*si);
        int lang_indx = lang_cc_cnt[type][nsi.m_language];
        lang_cc_cnt[type][nsi.m_language]++;
        nsi.m_language_index = lang_indx;
        m_tracks[(type) ? kTrackTypeCC708 : kTrackTypeCC608].push_back(nsi);
        int key = nsi.m_stream_id + ((type) ? 4 : -1);
        if (key < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "in_tracks key too small");
        }
        else
        {
            m_ccX08_in_tracks[key] = true;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("%1 caption service #%2 is in the %3 language.")
                .arg((type) ? "EIA-708" : "EIA-608")
                .arg(nsi.m_stream_id)
                .arg(iso639_key_toName(nsi.m_language)));
    }
}

void AvFormatDecoder::ScanTeletextCaptions(int av_index)
{
    // ScanStreams() calls m_tracks[kTrackTypeTeletextCaptions].clear()
    if (!m_ic->cur_pmt_sect || !m_tracks[kTrackTypeTeletextCaptions].empty())
        return;

    const ProgramMapTable pmt(PSIPTable(m_ic->cur_pmt_sect));

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        if (pmt.StreamType(i) != StreamID::PrivData)
            continue;

        const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::teletext);

        for (size_t j = 0; j < desc_list.size(); j++)
        {
            const TeletextDescriptor td(desc_list[j]);
            if (!td.IsValid())
                continue;
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
                    m_tracks[track].push_back(si);
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
        if (!m_tracks[kTrackTypeTeletextCaptions].empty())
            break;
    }
}

void AvFormatDecoder::ScanRawTextCaptions(int av_stream_index)
{
    AVDictionaryEntry *metatag =
        av_dict_get(m_ic->streams[av_stream_index]->metadata, "language", nullptr,
                    0);
    bool forced =
      (m_ic->streams[av_stream_index]->disposition & AV_DISPOSITION_FORCED) != 0;
    int lang = metatag ? get_canonical_lang(metatag->value) :
                         iso639_str3_to_key("und");
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Text Subtitle track #%1 is A/V stream #%2 "
                "and is in the %3 language(%4), forced=%5.")
                    .arg(m_tracks[kTrackTypeRawText].size()).arg(av_stream_index)
                    .arg(iso639_key_toName(lang)).arg(lang).arg(forced));
    StreamInfo si(av_stream_index, lang, 0, 0, 0, false, false, forced);
    m_tracks[kTrackTypeRawText].push_back(si);
}

/** \fn AvFormatDecoder::ScanDSMCCStreams(void)
 *  \brief Check to see whether there is a Network Boot Ifo sub-descriptor in the PMT which
 *         requires the MHEG application to reboot.
 */
void AvFormatDecoder::ScanDSMCCStreams(void)
{
    if (!m_ic || !m_ic->cur_pmt_sect)
        return;

    if (!m_itv && ! (m_itv = m_parent->GetInteractiveTV()))
        return;

    const ProgramMapTable pmt(PSIPTable(m_ic->cur_pmt_sect));

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        if (! StreamID::IsObjectCarousel(pmt.StreamType(i)))
            continue;

        LOG(VB_DSMCC, LOG_NOTICE, QString("ScanDSMCCStreams Found Object Carousel in Stream %1").arg(QString::number(i)));

        const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::data_broadcast_id);

        for (size_t j = 0; j < desc_list.size(); j++)
        {
            const unsigned char *desc = desc_list[j];
            desc++; // Skip tag
            uint length = *desc++;
            const unsigned char *endDesc = desc+length;
            uint dataBroadcastId = desc[0]<<8 | desc[1];
            LOG(VB_DSMCC, LOG_NOTICE, QString("ScanDSMCCStreams dataBroadcastId %1").arg(QString::number(dataBroadcastId)));
            if (dataBroadcastId != 0x0106) // ETSI/UK Profile
                continue;
            desc += 2; // Skip data ID
            while (desc != endDesc)
            {
                uint appTypeCode = desc[0]<<8 | desc[1];
                desc += 3; // Skip app type code and boot priority hint
                uint appSpecDataLen = *desc++;
#ifdef USING_MHEG
                LOG(VB_DSMCC, LOG_NOTICE, QString("ScanDSMCCStreams AppTypeCode %1").arg(QString::number(appTypeCode)));
                if (appTypeCode == 0x101) // UK MHEG profile
                {
                    const unsigned char *subDescEnd = desc + appSpecDataLen;
                    while (desc < subDescEnd)
                    {
                        uint sub_desc_tag = *desc++;
                        uint sub_desc_len = *desc++;
                        // Network boot info sub-descriptor.
                        if (sub_desc_tag == 1)
                            m_itv->SetNetBootInfo(desc, sub_desc_len);
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
    m_bitrate     = 0;

    m_tracks[kTrackTypeAttachment].clear();
    m_tracks[kTrackTypeAudio].clear();
    m_tracks[kTrackTypeSubtitle].clear();
    m_tracks[kTrackTypeTeletextCaptions].clear();
    m_tracks[kTrackTypeTeletextMenu].clear();
    m_tracks[kTrackTypeRawText].clear();
    if (!novideo)
    {
        // we will rescan video streams
        m_tracks[kTrackTypeVideo].clear();
        m_selectedTrack[kTrackTypeVideo].m_av_stream_index = -1;
        m_fps = 0;
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

    for (uint strm = 0; strm < m_ic->nb_streams; strm++)
    {
        AVCodecParameters *par = m_ic->streams[strm]->codecpar;
        AVCodecContext *enc = nullptr;

        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stream #%1, has id 0x%2 codec id %3, "
                    "type %4, bitrate %5 at 0x%6")
                .arg(strm).arg((uint64_t)m_ic->streams[strm]->id,0,16)
                .arg(ff_codec_id_string(par->codec_id))
                .arg(ff_codec_type_string(par->codec_type))
                .arg(par->bit_rate).arg((uint64_t)m_ic->streams[strm],0,16));

        switch (par->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                if (!par->codec_id)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Stream #%1 has an unknown video "
                                "codec id, skipping.").arg(strm));
                    continue;
                }

                // ffmpeg does not return a bitrate for several codecs and
                // formats. Forcing it to 500000 ensures the ringbuffer does not
                // use optimisations for low bitrate (audio and data) streams.
                if (par->bit_rate == 0)
                {
                    par->bit_rate = 500000;
                    unknownbitrate = true;
                }
                m_bitrate += par->bit_rate;

                break;
            }
            case AVMEDIA_TYPE_AUDIO:
            {
                enc = gCodecMap->hasCodecContext(m_ic->streams[strm]);
                if (enc && enc->internal)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Warning, audio codec 0x%1 id(%2) "
                                "type (%3) already open, leaving it alone.")
                            .arg((uint64_t)enc,0,16)
                            .arg(ff_codec_id_string(enc->codec_id))
                            .arg(ff_codec_type_string(enc->codec_type)));
                }
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("codec %1 has %2 channels")
                        .arg(ff_codec_id_string(par->codec_id))
                        .arg(par->channels));

                m_bitrate += par->bit_rate;
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (par->codec_id == AV_CODEC_ID_DVB_TELETEXT)
                    ScanTeletextCaptions(strm);
                if (par->codec_id == AV_CODEC_ID_TEXT)
                    ScanRawTextCaptions(strm);
                m_bitrate += par->bit_rate;

                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("subtitle codec (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_DATA:
            {
                ScanTeletextCaptions(strm);
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("data codec (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_ATTACHMENT:
            {
                if (par->codec_id == AV_CODEC_ID_TTF)
                   m_tracks[kTrackTypeAttachment].push_back(
                       StreamInfo(strm, 0, 0, m_ic->streams[strm]->id, 0));
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Attachment codec (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
            default:
            {
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Unknown codec type (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
        }

        if (par->codec_type != AVMEDIA_TYPE_AUDIO &&
            par->codec_type != AVMEDIA_TYPE_SUBTITLE)
            continue;

        // skip DVB teletext and text subs, there is no libavcodec decoder
        if (par->codec_type == AVMEDIA_TYPE_SUBTITLE &&
           (par->codec_id   == AV_CODEC_ID_DVB_TELETEXT ||
            par->codec_id   == AV_CODEC_ID_TEXT))
            continue;

        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Looking for decoder for %1")
                .arg(ff_codec_id_string(par->codec_id)));

        if (par->codec_id == AV_CODEC_ID_PROBE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Probing of stream #%1 unsuccesful, ignoring.").arg(strm));
            continue;
        }

        if (!enc)
            enc = gCodecMap->getCodecContext(m_ic->streams[strm]);

        const AVCodec *codec = nullptr;
        if (enc)
            codec = enc->codec;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not find decoder for codec (%1), ignoring.")
                    .arg(ff_codec_id_string(par->codec_id)));

            // Nigel's bogus codec-debug. Dump the list of codecs & decoders,
            // and have one last attempt to find a decoder. This is usually
            // only caused by build problems, where libavcodec needs a rebuild
            if (VERBOSE_LEVEL_CHECK(VB_LIBAV, LOG_ANY))
            {
                AVCodec *p = av_codec_next(nullptr);
                int      i = 1;
                while (p)
                {
                    QString msg;

                    if (p->name[0] != '\0')
                        msg = QString("Codec %1:").arg(p->name);
                    else
                        msg = QString("Codec %1, null name,").arg(strm);

                    if (p->decode == nullptr)
                        msg += "decoder is null";

                    LOG(VB_LIBAV, LOG_INFO, LOC + msg);

                    if (p->id == par->codec_id)
                    {
                        codec = p;
                        break;
                    }

                    LOG(VB_LIBAV, LOG_INFO, LOC +
                        QString("Codec 0x%1 != 0x%2") .arg(p->id, 0, 16)
                            .arg(par->codec_id, 0, 16));
                    p = av_codec_next(p);
                    ++i;
                }
            }
            if (codec)
                enc = gCodecMap->getCodecContext(m_ic->streams[strm], codec);
            else
                continue;
        }
        if (!enc)
            continue;

        if (enc->codec && par->codec_id != enc->codec_id)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Already opened codec not matching (%1 vs %2). Reopening")
                .arg(ff_codec_id_string(enc->codec_id))
                .arg(ff_codec_id_string(enc->codec->id)));
            gCodecMap->freeCodecContext(m_ic->streams[strm]);
            enc = gCodecMap->getCodecContext(m_ic->streams[strm]);
        }
        if (!OpenAVCodec(enc, codec))
            continue;
        if (!enc)
            continue;

        if (!IsValidStream(m_ic->streams[strm]->id))
        {
            /* Hide this stream if it's not valid in this context.
             * This can happen, for example, on a Blu-ray disc if there
             * are more physical streams than there is metadata about them.
             * (e.g. Despicable Me)
             */
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Stream 0x%1 is not valid in this context - skipping")
                .arg(m_ic->streams[strm]->id, 4, 16));
            continue;
        }

        if (par->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            bool forced = (m_ic->streams[strm]->disposition & AV_DISPOSITION_FORCED) != 0;
            int lang = GetSubtitleLanguage(subtitleStreamCount, strm);
            int lang_indx = lang_sub_cnt[lang]++;
            subtitleStreamCount++;

            m_tracks[kTrackTypeSubtitle].push_back(
                StreamInfo(strm, lang, lang_indx, m_ic->streams[strm]->id, 0, 0, false, false, forced));

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Subtitle track #%1 is A/V stream #%2 "
                        "and is in the %3 language(%4).")
                    .arg(m_tracks[kTrackTypeSubtitle].size()).arg(strm)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }

        if (par->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            int lang = GetAudioLanguage(audioStreamCount, strm);
            AudioTrackType type = GetAudioTrackType(strm);
            int channels  = par->channels;
            int lang_indx = lang_aud_cnt[lang]++;
            audioStreamCount++;

            if (enc->avcodec_dual_language)
            {
                m_tracks[kTrackTypeAudio].push_back(
                    StreamInfo(strm, lang, lang_indx, m_ic->streams[strm]->id, channels,
                               false, false, false, type));
                lang_indx = lang_aud_cnt[lang]++;
                m_tracks[kTrackTypeAudio].push_back(
                    StreamInfo(strm, lang, lang_indx, m_ic->streams[strm]->id, channels,
                               true, false, false, type));
            }
            else
            {
                int logical_stream_id;
                if (ringBuffer && ringBuffer->IsDVD())
                {
                    logical_stream_id = ringBuffer->DVD()->GetAudioTrackNum(m_ic->streams[strm]->id);
                    channels = ringBuffer->DVD()->GetNumAudioChannels(logical_stream_id);
                }
                else
                    logical_stream_id = m_ic->streams[strm]->id;

                if (logical_stream_id == -1)
                {
                    // This stream isn't mapped, so skip it
                    continue;
                }

                m_tracks[kTrackTypeAudio].push_back(
                   StreamInfo(strm, lang, lang_indx, logical_stream_id, channels,
                              false, false, false, type));
            }

            LOG(VB_AUDIO, LOG_INFO, LOC +
                QString("Audio Track #%1, of type (%2) is A/V stream #%3 (id=0x%4) "
                        "and has %5 channels in the %6 language(%7).")
                    .arg(m_tracks[kTrackTypeAudio].size()).arg(toString(type))
                    .arg(strm).arg(m_ic->streams[strm]->id,0,16).arg(enc->channels)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }
    }

    // Now find best video track to play
    if (!novideo && m_ic)
    {
        for(;;)
        {
            AVCodec *codec = nullptr;
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Trying to select best video track");

            /*
             *  Find the "best" stream in the file.
             *
             * The best stream is determined according to various heuristics as
             * the most likely to be what the user expects. If the decoder parameter
             * is not nullptr, av_find_best_stream will find the default decoder
             * for the stream's codec; streams for which no decoder can be found
             * are ignored.
             *
             * If av_find_best_stream returns successfully and decoder_ret is not nullptr,
             * then *decoder_ret is guaranteed to be set to a valid AVCodec.
             */
            int selTrack = av_find_best_stream(m_ic, AVMEDIA_TYPE_VIDEO,
                                               -1, -1, &codec, 0);

            if (selTrack < 0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "No video track found/selected.");
                break;
            }

            if (m_averror_count > SEQ_PKT_ERR_MAX)
                gCodecMap->freeCodecContext(m_ic->streams[selTrack]);
            AVCodecContext *enc = gCodecMap->getCodecContext(m_ic->streams[selTrack], codec);
            StreamInfo si(selTrack, 0, 0, 0, 0);

            m_tracks[kTrackTypeVideo].push_back(si);
            m_selectedTrack[kTrackTypeVideo] = si;

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Selected track #%1 (id 0x%2 codec id %3, "
                        "type %4, bitrate %5 at 0x%6)")
                .arg(selTrack).arg((uint64_t)m_ic->streams[selTrack]->id,0,16)
                .arg(ff_codec_id_string(enc->codec_id))
                .arg(ff_codec_type_string(enc->codec_type))
                .arg(enc->bit_rate).arg((uint64_t)m_ic->streams[selTrack],0,16));

            m_codec_is_mpeg = CODEC_IS_FFMPEG_MPEG(enc->codec_id);

            delete m_private_dec;
            m_private_dec = nullptr;
            m_h264_parser->Reset();

            QSize dim = get_video_dim(*enc);
            uint width  = max(dim.width(),  16);
            uint height = max(dim.height(), 16);
            QString dec = "ffmpeg";
            uint thread_count = 1;
            QString codecName;
            if (enc->codec)
                codecName = enc->codec->name;
            if (enc->framerate.den && enc->framerate.num)
                m_fps = float(enc->framerate.num) / float(enc->framerate.den);
            else
                m_fps = 0;
            if (!m_is_db_ignored)
            {
                // VideoDisplayProfile vdp;
                m_videoDisplayProfile.SetInput(QSize(width, height),m_fps,codecName);
                dec = m_videoDisplayProfile.GetDecoder();
                thread_count = m_videoDisplayProfile.GetMaxCPUs();
                bool skip_loop_filter = m_videoDisplayProfile.IsSkipLoopEnabled();
                if  (!skip_loop_filter)
                {
                    enc->skip_loop_filter = AVDISCARD_NONKEY;
                }
            }

            m_video_codec_id = kCodec_NONE;
            int version = mpeg_version(enc->codec_id);
            if (version)
                m_video_codec_id = (MythCodecID)(kCodec_MPEG1 + version - 1);

                // Check it's a codec we can decode using GPU
#ifdef USING_OPENMAX
            // The OpenMAX decoder supports H264 high 10, 422 and 444 profiles
            if (dec != "openmax")
#endif
            if (m_averror_count > SEQ_PKT_ERR_MAX || force_sw_decode(enc))
            {
                bool wasgpu = dec != "ffmpeg";
                if (FlagIsSet(kDecodeAllowGPU) && force_sw_decode(enc) && wasgpu)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Unsupported Video Profile - forcing software decode");
                }
                if (FlagIsSet(kDecodeAllowGPU) && m_averror_count > SEQ_PKT_ERR_MAX && wasgpu)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "GPU decoding failed - forcing software decode");
                }
                m_averror_count = 0;
                dec = "ffmpeg";
            }

            // NOLINTNEXTLINE(readability-misleading-indentation)
            if (version && FlagIsSet(kDecodeAllowGPU))
            {
                bool foundgpudecoder = false;
		Q_UNUSED(foundgpudecoder); // Prevent warning if no GPU decoders

#ifdef USING_VDPAU
                MythCodecID vdpau_mcid;
                vdpau_mcid =
                    VideoOutputVDPAU::GetBestSupportedCodec(width, height, dec,
                                                            mpeg_version(enc->codec_id),
                                                            false);

                if (codec_is_vdpau(vdpau_mcid))
                {
                    m_video_codec_id = vdpau_mcid;
                    // cppcheck-suppress unreadVariable
                    foundgpudecoder = true;
                }
#endif // USING_VDPAU
#ifdef USING_GLVAAPI
                if (!foundgpudecoder)
                {
                    MythCodecID vaapi_mcid;
                    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
                    vaapi_mcid =
                        VideoOutputOpenGLVAAPI::GetBestSupportedCodec(width, height, dec,
                                                                      mpeg_version(enc->codec_id),
                                                                      false,
                                                                      pix_fmt);

                    if (codec_is_vaapi(vaapi_mcid))
                    {
                        m_video_codec_id = vaapi_mcid;
                        enc->pix_fmt = pix_fmt;
                        foundgpudecoder = true;
                    }
                }
#endif // USING_GLVAAPI
#ifdef USING_DXVA2
                if (!foundgpudecode)
                {
                    MythCodecID dxva2_mcid;
                    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
                    dxva2_mcid = VideoOutputD3D::GetBestSupportedCodec(
                        width, height, dec, mpeg_version(enc->codec_id),
                        false, pix_fmt);

                    if (codec_is_dxva2(dxva2_mcid))
                    {
                        m_video_codec_id = dxva2_mcid;
                        enc->pix_fmt = pix_fmt;
                        // cppcheck-suppress unreadVariable
                        foundgpudecoder = true;
                    }
                }
#endif // USING_DXVA2
#ifdef USING_MEDIACODEC
                if (!foundgpudecoder)
                {
                    MythCodecID mediacodec_mcid;
                    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
                    mediacodec_mcid = MediaCodecContext::GetBestSupportedCodec(
                        &codec, dec, mpeg_version(enc->codec_id),
                        pix_fmt);

                    if (codec_is_mediacodec(mediacodec_mcid))
                    {
                        gCodecMap->freeCodecContext(m_ic->streams[selTrack]);
                        enc = gCodecMap->getCodecContext(m_ic->streams[selTrack], codec);
                        m_video_codec_id = mediacodec_mcid;
                        foundgpudecoder = true;
                    }
                }
#endif // USING_MEDIACODEC
#ifdef USING_VAAPI2
                if (!foundgpudecoder)
                {
                    MythCodecID vaapi2_mcid;
                    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
                    vaapi2_mcid = Vaapi2Context::GetBestSupportedCodec(
                        &codec, dec, mpeg_version(enc->codec_id),
                        pix_fmt);

                    if (codec_is_vaapi2(vaapi2_mcid))
                    {
                        gCodecMap->freeCodecContext(m_ic->streams[selTrack]);
                        enc = gCodecMap->getCodecContext(m_ic->streams[selTrack], codec);
                        m_video_codec_id = vaapi2_mcid;
                        foundgpudecoder = true;
                    }
                }
#endif // USING_VAAPI2
#ifdef USING_NVDEC
                if (!foundgpudecoder)
                {
                    MythCodecID nvdec_mcid;
                    AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
                    nvdec_mcid = NvdecContext::GetBestSupportedCodec(
                        &codec, dec, mpeg_version(enc->codec_id),
                        pix_fmt);

                    if (codec_is_nvdec(nvdec_mcid))
                    {
                        gCodecMap->freeCodecContext(m_ic->streams[selTrack]);
                        enc = gCodecMap->getCodecContext(m_ic->streams[selTrack], codec);
                        m_video_codec_id = nvdec_mcid;
                        //foundgpudecoder = true;
                    }
                }
#endif // USING_NVDEC
            }
            // default to mpeg2
            if (m_video_codec_id == kCodec_NONE)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Unknown video codec - defaulting to MPEG2");
                m_video_codec_id = kCodec_MPEG2;
            }

            // Use a PrivateDecoder if allowed in playerFlags AND matched
            // via the decoder name
            m_private_dec = PrivateDecoder::Create(dec, playerFlags, enc);
            if (m_private_dec)
                thread_count = 1;

            m_use_frame_timing = false;
            if (! ringBuffer->IsDVD()
                && (codec_is_std(m_video_codec_id)
                    || codec_is_mediacodec(m_video_codec_id)
                    || codec_is_vaapi2(m_video_codec_id)
                    || codec_is_nvdec(m_video_codec_id)
                    || GetCodecDecoderName() == "openmax"))
                m_use_frame_timing = true;

            if (FlagIsSet(kDecodeSingleThreaded))
                thread_count = 1;

            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Using %1 CPUs for decoding")
                .arg(HAVE_THREADS ? thread_count : 1));

            if (HAVE_THREADS)
                enc->thread_count = thread_count;

            InitVideoCodec(m_ic->streams[selTrack], enc, true);

            ScanATSCCaptionStreams(selTrack);
            UpdateATSCCaptionTracks();

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Using %1 for video decoding")
                .arg(GetCodecDecoderName()));

            {
                QMutexLocker locker(avcodeclock);

                if (!OpenAVCodec(enc, codec))
                {
                    scanerror = -1;
                    break;
                }
            }

            break;
        }
    }

    if (m_ic && ((uint)m_ic->bit_rate > m_bitrate))
        m_bitrate = (uint)m_ic->bit_rate;

    if (m_bitrate > 0)
    {
        m_bitrate = (m_bitrate + 999) / 1000;
        if (ringBuffer)
            ringBuffer->UpdateRawBitrate(m_bitrate);
    }

    // update RingBuffer buffer size
    if (ringBuffer)
    {
        ringBuffer->SetBufferSizeFactors(unknownbitrate,
                        m_ic && QString(m_ic->iformat->name).contains("matroska"));
    }

    PostProcessTracks();

    // Select a new track at the next opportunity.
    ResetTracks();

    // We have to do this here to avoid the NVP getting stuck
    // waiting on audio.
    if (m_audio->HasAudioIn() && m_tracks[kTrackTypeAudio].empty())
    {
        m_audio->SetAudioParams(FORMAT_NONE, -1, -1, AV_CODEC_ID_NONE, -1, false);
        m_audio->ReinitAudio();
        if (ringBuffer && ringBuffer->IsDVD())
            m_audioIn = AudioInfo();
    }

    // if we don't have a video stream we still need to make sure some
    // video params are set properly
    if (m_selectedTrack[kTrackTypeVideo].m_av_stream_index == -1)
    {
        QString tvformat = gCoreContext->GetSetting("TVFormat").toLower();
        if (tvformat == "ntsc" || tvformat == "ntsc-jp" ||
            tvformat == "pal-m" || tvformat == "atsc")
        {
            m_fps = 29.97;
            m_parent->SetVideoParams(-1, -1, 29.97);
        }
        else
        {
            m_fps = 25.0;
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

#ifdef USING_MEDIACODEC
    if (QString("mediacodec") == codec->wrapper_name)
        av_jni_set_java_vm(QAndroidJniEnvironment::javaVM(), nullptr);
#endif
    int ret = avcodec_open2(avctx, codec, nullptr);
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

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Opened codec 0x%1, id(%2) type(%3)")
        .arg((uint64_t)avctx,0,16)
        .arg(ff_codec_id_string(avctx->codec_id))
        .arg(ff_codec_type_string(avctx->codec_type)));
    return true;
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
}

///Returns TeleText language
int AvFormatDecoder::GetTeletextLanguage(uint lang_idx) const
{
    for (uint i = 0; i < (uint) m_tracks[kTrackTypeTeletextCaptions].size(); i++)
    {
        if (m_tracks[kTrackTypeTeletextCaptions][i].m_language_index == lang_idx)
        {
             return m_tracks[kTrackTypeTeletextCaptions][i].m_language;
        }
    }

    return iso639_str3_to_key("und");
}
/// Returns DVD Subtitle language
int AvFormatDecoder::GetSubtitleLanguage(uint subtitle_index, uint stream_index)
{
    (void)subtitle_index;
    AVDictionaryEntry *metatag =
        av_dict_get(m_ic->streams[stream_index]->metadata, "language", nullptr, 0);
    return metatag ? get_canonical_lang(metatag->value) :
                     iso639_str3_to_key("und");
}

/// Return ATSC Closed Caption Language
int AvFormatDecoder::GetCaptionLanguage(TrackTypes trackType, int service_num)
{
    int ret = -1;
    for (uint i = 0; i < (uint) m_pmt_track_types.size(); i++)
    {
        if ((m_pmt_track_types[i] == trackType) &&
            (m_pmt_tracks[i].m_stream_id == service_num))
        {
            ret = m_pmt_tracks[i].m_language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    for (uint i = 0; i < (uint) m_stream_track_types.size(); i++)
    {
        if ((m_stream_track_types[i] == trackType) &&
            (m_stream_tracks[i].m_stream_id == service_num))
        {
            ret = m_stream_tracks[i].m_language;
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
    AVStream *stream = m_ic->streams[stream_index];

    if (m_ic->cur_pmt_sect) // mpeg-ts
    {
        const ProgramMapTable pmt(PSIPTable(m_ic->cur_pmt_sect));
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

    // Find the position of the streaminfo in m_tracks[kTrackTypeAudio]
    sinfo_vec_t::iterator current = m_tracks[kTrackTypeAudio].begin();
    for (; current != m_tracks[kTrackTypeAudio].end(); ++current)
    {
        if (current->m_av_stream_index == streamIndex)
            break;
    }

    if (current == m_tracks[kTrackTypeAudio].end())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Invalid stream index passed to "
                    "SetupAudioStreamSubIndexes: %1").arg(streamIndex));

        return;
    }

    // Remove the extra substream or duplicate the current substream
    sinfo_vec_t::iterator next = current + 1;
    if (current->m_av_substream_index == -1)
    {
        // Split stream in two (Language I + Language II)
        StreamInfo lang1 = *current;
        StreamInfo lang2 = *current;
        lang1.m_av_substream_index = 0;
        lang2.m_av_substream_index = 1;
        *current = lang1;
        m_tracks[kTrackTypeAudio].insert(next, lang2);
        return;
    }

    if ((next == m_tracks[kTrackTypeAudio].end()) ||
        (next->m_av_stream_index != streamIndex))
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
    stream.m_av_substream_index = -1;
    *current = stream;
    m_tracks[kTrackTypeAudio].erase(next);
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
    for (uint i = 0; i < m_ic->nb_streams;)
    {
        AVStream *st = m_ic->streams[i];
        AVCodecContext *avctx = gCodecMap->hasCodecContext(st);
        if (avctx && avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            gCodecMap->freeCodecContext(st);
            av_remove_stream(m_ic, st->id, 0);
            i--;
        }
        else
            i++;
    }
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic, int flags)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);

    if (!IS_DR1_PIX_FMT(c->pix_fmt))
    {
        nd->m_directrendering = false;
        return avcodec_default_get_buffer2(c, pic, flags);
    }
    nd->m_directrendering = true;

    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    if (!frame)
        return -1;

    for (int i = 0; i < 3; i++)
    {
        pic->data[i]     = frame->buf + frame->offsets[i];
        pic->linesize[i] = frame->pitches[i];
    }

    pic->opaque = frame;
    pic->reordered_opaque = c->reordered_opaque;

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)frame, 0, release_avf_buffer, nd, 0);
    pic->buf[0] = buffer;

    return 0;
}

void release_avf_buffer(void *opaque, uint8_t *data)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)opaque;
    VideoFrame *frame   = (VideoFrame*)data;

    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame(frame);
}

#if defined(USING_VAAPI2) || defined(USING_NVDEC)
static void dummy_release_avf_buffer(void * /*opaque*/, uint8_t * /*data*/)
{
}
#endif

#ifdef USING_VDPAU
int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic, int /*flags*/)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame   = nd->GetPlayer()->GetNextVideoFrame();

    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    render->state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;

    for (int i = 0; i < 4; i++)
    {
        pic->data[i]     = nullptr;
        pic->linesize[i] = 0;
    }
    pic->opaque     = frame;
    pic->data[3]    = (uint8_t*)(uintptr_t)render->surface;
    frame->pix_fmt  = c->pix_fmt;
    pic->reordered_opaque = c->reordered_opaque;

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)frame, 0, release_avf_buffer_vdpau, nd, 0);
    pic->buf[0] = buffer;

    return 0;
}

void release_avf_buffer_vdpau(void *opaque, uint8_t *data)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)opaque;
    VideoFrame *frame   = (VideoFrame*)data;

    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
    render->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;

    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame(frame);
}

int render_wrapper_vdpau(struct AVCodecContext *s, AVFrame *src,
                         const VdpPictureInfo *info,
                         uint32_t count,
                         const VdpBitstreamBuffer *buffers)
{
    if (!src)
        return -1;

    if (s && src && s->opaque && src->opaque)
    {
        AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);
        VideoFrame *frame = (VideoFrame *)src->opaque;
        struct vdpau_render_state data {};

        data.surface = (VdpVideoSurface)(uintptr_t)src->data[3];
        data.bitstream_buffers_used = count;
        data.bitstream_buffers = (VdpBitstreamBuffer*)buffers;

        // Store information we will require in DrawSlice()
        frame->priv[0] = (unsigned char*)&data;
        frame->priv[1] = (unsigned char*)info;

        nd->GetPlayer()->DrawSlice(frame, 0, 0, 0, 0);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "render_wrapper_vdpau called with bad avctx or src");
    }

    return 0;
}
#endif // USING_VDPAU

#ifdef USING_DXVA2
int get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic, int /*flags*/)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    for (int i = 0; i < 4; i++)
    {
        pic->data[i]     = nullptr;
        pic->linesize[i] = 0;
    }
    pic->opaque      = frame;
    frame->pix_fmt   = c->pix_fmt;
    pic->reordered_opaque = c->reordered_opaque;
    pic->data[0] = (uint8_t*)frame->buf;
    pic->data[3] = (uint8_t*)frame->buf;

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)frame, 0, release_avf_buffer, nd, 0);
    pic->buf[0] = buffer;

    return 0;
}
#endif

#ifdef USING_VAAPI
int get_avf_buffer_vaapi(struct AVCodecContext *c, AVFrame *pic, int /*flags*/)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    VideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    for (int i = 0; i < 4; i++)
    {
        pic->data[i]     = nullptr;
        pic->linesize[i] = 0;
    }
    pic->opaque      = frame;
    frame->pix_fmt   = c->pix_fmt;

    if (nd->GetPlayer())
    {
        nd->GetPlayer()->GetDecoderContext(frame->buf, pic->data[3]);
    }

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)frame, 0, release_avf_buffer, nd, 0);
    pic->buf[0] = buffer;

    return 0;
}
#endif

#ifdef USING_VAAPI2
int get_avf_buffer_vaapi2(struct AVCodecContext *c, AVFrame *pic, int flags)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    nd->m_directrendering = false;
    return avcodec_default_get_buffer2(c, pic, flags);
}
#endif
#ifdef USING_NVDEC
int get_avf_buffer_nvdec(struct AVCodecContext *c, AVFrame *pic, int flags)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    nd->m_directrendering = false;
    return avcodec_default_get_buffer2(c, pic, flags);
}
#endif

void AvFormatDecoder::DecodeDTVCC(const uint8_t *buf, uint buf_size, bool scte)
{
    if (!buf_size)
        return;

    // closed caption data
    //cc_data() {
    // reserved                1 0.0   1
    // process_cc_data_flag    1 0.1   bslbf
    bool process_cc_data = (buf[0] & 0x40) != 0;
    if (!process_cc_data)
        return; // early exit if process_cc_data_flag false

    // additional_data_flag    1 0.2   bslbf
    //bool additional_data = buf[0] & 0x20;
    // cc_count                5 0.3   uimsbf
    uint cc_count = buf[0] & 0x1f;
    // em_data                 8 1.0

    if (buf_size < 2+(3*cc_count))
        return;

    DecodeCCx08(buf+2, cc_count*3, scte);
}

void AvFormatDecoder::DecodeCCx08(const uint8_t *buf, uint buf_size, bool scte)
{
    if (buf_size < 3)
        return;

    bool had_608 = false, had_708 = false;
    for (uint cur = 0; cur + 3 < buf_size; cur += 3)
    {
        uint cc_code  = buf[cur];
        bool cc_valid = (cc_code & 0x04) != 0U;

        uint data1    = buf[cur+1];
        uint data2    = buf[cur+2];
        uint data     = (data2 << 8) | data1;
        uint cc_type  = cc_code & 0x03;

        if (!cc_valid)
        {
            if (cc_type >= 0x2)
                m_ccd708->decode_cc_null();
            continue;
        }

        if (scte || cc_type <= 0x1) // EIA-608 field-1/2
        {
            uint field;
            if (cc_type == 0x2)
            {
                // SCTE repeated field
                field = (m_last_scte_field == 0 ? 1 : 0);
                m_invert_scte_field = (m_invert_scte_field == 0 ? 1 : 0);
            }
            else
            {
                field = cc_type ^ m_invert_scte_field;
            }

            if (cc608_good_parity(m_cc608_parity_table, data))
            {
                // in film mode, we may start at the wrong field;
                // correct if XDS start/cont/end code is detected
                // (must be field 2)
                if (scte && field == 0 &&
                    (data1 & 0x7f) <= 0x0f && (data1 & 0x7f) != 0x00)
                {
                    if (cc_type == 1)
                        m_invert_scte_field = 0;
                    field = 1;

                    // flush decoder
                    m_ccd608->FormatCC(0, -1, -1);
                }

                had_608 = true;
                m_ccd608->FormatCCField(m_lastccptsu / 1000, field, data);

                m_last_scte_field = field;
            }
        }
        else
        {
            had_708 = true;
            m_ccd708->decode_cc_data(cc_type, data1, data2);
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
        m_ccd608->GetServices(15/*seconds*/, seen_608);
        for (uint i = 0; i < 4; i++)
        {
            need_change_608 |= (seen_608[i] && !m_ccX08_in_tracks[i]) ||
                (!seen_608[i] && m_ccX08_in_tracks[i] && !m_ccX08_in_pmt[i]);
        }
    }

    bool need_change_708 = false;
    bool seen_708[64];
    if (check_708 || need_change_608)
    {
        m_ccd708->services(15/*seconds*/, seen_708);
        for (uint i = 1; i < 64 && !need_change_608 && !need_change_708; i++)
        {
            need_change_708 |= (seen_708[i] && !m_ccX08_in_tracks[i+4]) ||
                (!seen_708[i] && m_ccX08_in_tracks[i+4] && !m_ccX08_in_pmt[i+4]);
        }
        if (need_change_708 && !check_608)
            m_ccd608->GetServices(15/*seconds*/, seen_608);
    }

    if (!need_change_608 && !need_change_708)
        return;

    ScanATSCCaptionStreams(m_selectedTrack[kTrackTypeVideo].m_av_stream_index);

    m_stream_tracks.clear();
    m_stream_track_types.clear();
    int av_index = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    int lang = iso639_str3_to_key("und");
    for (uint i = 1; i < 64; i++)
    {
        if (seen_708[i] && !m_ccX08_in_pmt[i+4])
        {
            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i, 0, false/*easy*/, true/*wide*/);
            m_stream_tracks.push_back(si);
            m_stream_track_types.push_back(kTrackTypeCC708);
        }
    }
    for (uint i = 0; i < 4; i++)
    {
        if (seen_608[i] && !m_ccX08_in_pmt[i])
        {
            if (0==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 1);
            else if (2==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 2);
            else
                lang = iso639_str3_to_key("und");

            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i+1, 0, false/*easy*/, false/*wide*/);
            m_stream_tracks.push_back(si);
            m_stream_track_types.push_back(kTrackTypeCC608);
        }
    }
    UpdateATSCCaptionTracks();
}

void AvFormatDecoder::HandleGopStart(
    AVPacket *pkt, bool can_reliably_parse_keyframes)
{
    if (m_prevgoppos != 0 && m_keyframedist != 1)
    {
        int tempKeyFrameDist = m_framesRead - 1 - m_prevgoppos;
        bool reset_kfd = false;

        if (!m_gopset || m_livetv) // gopset: we've seen 2 keyframes
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "gopset not set, syncing positionMap");
            SyncPositionMap();
            if (tempKeyFrameDist > 0 && !m_livetv)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Initial key frame distance: %1.")
                        .arg(m_keyframedist));
                m_gopset     = true;
                reset_kfd    = true;
            }
        }
        else if (m_keyframedist != tempKeyFrameDist && tempKeyFrameDist > 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Key frame distance changed from %1 to %2.")
                    .arg(m_keyframedist).arg(tempKeyFrameDist));
            reset_kfd = true;
        }

        if (reset_kfd)
        {
            m_keyframedist    = tempKeyFrameDist;
            m_maxkeyframedist = max(m_keyframedist, m_maxkeyframedist);

            m_parent->SetKeyframeDistance(m_keyframedist);

#if 0
            // also reset length
            QMutexLocker locker(&m_positionMapLock);
            if (!m_positionMap.empty())
            {
                long long index       = m_positionMap.back().index;
                long long totframes   = index * m_keyframedist;
                uint length = (uint)((totframes * 1.0F) / m_fps);
                m_parent->SetFileLength(length, totframes);
            }
#endif
        }
    }

    m_lastKey = m_prevgoppos = m_framesRead - 1;

    if (can_reliably_parse_keyframes &&
        !m_hasFullPositionMap && !m_livetv && !m_watchingrecording)
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
                .arg(m_framesRead) .arg(last_frame) .arg(m_keyframedist));
#endif

        // if we don't have an entry, fill it in with what we've just parsed
        if (m_framesRead > last_frame && m_keyframedist > 0)
        {
            long long startpos = pkt->pos;

            LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                QString("positionMap[ %1 ] == %2.")
                    .arg(m_framesRead).arg(startpos));

            PosMapEntry entry = {m_framesRead, m_framesRead, startpos};

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
            if (m_trackTotalDuration)
            {
                m_frameToDurMap[m_framesRead] =
                    llround(m_totalDuration.num * 1000.0 / m_totalDuration.den);
                m_durToFrameMap[m_frameToDurMap[m_framesRead]] = m_framesRead;
            }
        }

#if 0
        // If we are > 150 frames in and saw no positionmap at all, reset
        // length based on the actual bitrate seen so far
        if (m_framesRead > 150 && !m_recordingHasPositionMap && !m_livetv)
        {
            m_bitrate = (int)((pkt->pos * 8 * m_fps) / (m_framesRead - 1));
            float bytespersec = (float)m_bitrate / 8;
            float secs = ringBuffer->GetRealFileSize() * 1.0 / bytespersec;
            m_parent->SetFileLength((int)(secs), (int)(secs * m_fps));
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
    AVCodecContext *context = gCodecMap->getCodecContext(stream);
    const uint8_t *bufptr = pkt->data;
    const uint8_t *bufend = pkt->data + pkt->size;

    while (bufptr < bufend)
    {
        bufptr = avpriv_find_start_code(bufptr, bufend, &m_start_code_state);

        float aspect_override = -1.0F;
        if (ringBuffer->IsDVD())
            aspect_override = ringBuffer->DVD()->GetAspectOverride();

        if (m_start_code_state >= SLICE_MIN && m_start_code_state <= SLICE_MAX)
            continue;
        if (SEQ_START == m_start_code_state)
        {
            if (bufptr + 11 >= pkt->data + pkt->size)
                continue; // not enough valid data...
            SequenceHeader *seq = reinterpret_cast<SequenceHeader*>(
                const_cast<uint8_t*>(bufptr));

            uint  width  = seq->width()  >> context->lowres;
            uint  height = seq->height() >> context->lowres;
            m_current_aspect = seq->aspect(context->codec_id ==
                                         AV_CODEC_ID_MPEG1VIDEO);
            if (stream->sample_aspect_ratio.num)
                m_current_aspect = av_q2d(stream->sample_aspect_ratio) *
                    width / height;
            if (aspect_override > 0.0F)
                m_current_aspect = aspect_override;
            float seqFPS = seq->fps();

            bool changed =
                (seqFPS > static_cast<float>(m_fps)+0.01F) ||
                (seqFPS < static_cast<float>(m_fps)-0.01F);
            changed |= (width  != (uint)m_current_width );
            changed |= (height != (uint)m_current_height);

            if (changed)
            {
                if (m_private_dec)
                    m_private_dec->Reset();

                m_parent->SetVideoParams(width, height, seqFPS, kScan_Detect);

                m_current_width  = width;
                m_current_height = height;
                m_fps            = seqFPS;

                m_gopset = false;
                m_prevgoppos = 0;
                m_firstvpts = m_lastapts = m_lastvpts = m_lastccptsu = 0;
                m_firstvptsinuse = true;
                m_faulty_pts = m_faulty_dts = 0;
                m_last_pts_for_fault_detection = 0;
                m_last_dts_for_fault_detection = 0;
                m_pts_detected = false;
                m_reordered_pts_detected = false;

                // fps debugging info
                float avFPS = normalized_fps(stream, context);
                if ((seqFPS > avFPS+0.01F) || (seqFPS < avFPS-0.01F))
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("avFPS(%1) != seqFPS(%2)")
                            .arg(avFPS).arg(seqFPS));
                }
            }

            m_seq_count++;

            if (!m_seen_gop && m_seq_count > 1)
            {
                HandleGopStart(pkt, true);
                pkt->flags |= AV_PKT_FLAG_KEY;
            }
        }
        else if (GOP_START == m_start_code_state)
        {
            HandleGopStart(pkt, true);
            m_seen_gop = true;
            pkt->flags |= AV_PKT_FLAG_KEY;
        }
    }
}

// Returns the number of frame starts identified in the packet.
int AvFormatDecoder::H264PreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = gCodecMap->getCodecContext(stream);
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

        m_current_aspect = get_aspect(*m_h264_parser);
        uint  width  = m_h264_parser->pictureWidthCropped();
        uint  height = m_h264_parser->pictureHeightCropped();
        float seqFPS = m_h264_parser->frameRate();

        bool res_changed = ((width  != (uint)m_current_width) ||
                            (height != (uint)m_current_height));
        bool fps_changed =
            (seqFPS > 0.0F) &&
            ((seqFPS > static_cast<float>(m_fps) + 0.01F) ||
             (seqFPS < static_cast<float>(m_fps) - 0.01F));

        if (fps_changed || res_changed)
        {
            if (m_private_dec)
                m_private_dec->Reset();

            m_parent->SetVideoParams(width, height, seqFPS, kScan_Detect);

            m_current_width  = width;
            m_current_height = height;

            if (seqFPS > 0.0F)
                m_fps = seqFPS;

            m_gopset = false;
            m_prevgoppos = 0;
            m_firstvpts = m_lastapts = m_lastvpts = m_lastccptsu = 0;
            m_firstvptsinuse = true;
            m_faulty_pts = m_faulty_dts = 0;
            m_last_pts_for_fault_detection = 0;
            m_last_dts_for_fault_detection = 0;
            m_pts_detected = false;
            m_reordered_pts_detected = false;

            // fps debugging info
            float avFPS = normalized_fps(stream, context);
            if ((seqFPS > avFPS+0.01F) || (seqFPS < avFPS-0.01F))
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("avFPS(%1) != seqFPS(%2)")
                        .arg(avFPS).arg(seqFPS));
            }

        }

        HandleGopStart(pkt, true);
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    return num_frames;
}

bool AvFormatDecoder::PreProcessVideoPacket(AVStream *curstream, AVPacket *pkt)
{
    AVCodecContext *context = gCodecMap->getCodecContext(curstream);
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
            m_seen_gop = true;
        }
        else
        {
            m_seq_count++;
            if (!m_seen_gop && m_seq_count > 1)
            {
                HandleGopStart(pkt, false);
            }
        }
    }

    if (m_framesRead == 0 && !m_justAfterChange &&
        !(pkt->flags & AV_PKT_FLAG_KEY))
    {
        av_packet_unref(pkt);
        return false;
    }

    m_framesRead += num_frames;

    if (m_trackTotalDuration)
    {
        // The ffmpeg libraries represent a frame interval of a
        // 59.94fps video as 1501/90000 seconds, when it should
        // actually be 1501.5/90000 seconds.
        AVRational pkt_dur = AVRationalInit(pkt->duration);
        pkt_dur = av_mul_q(pkt_dur, curstream->time_base);
        if (pkt_dur.num == 1501 && pkt_dur.den == 90000)
            pkt_dur = AVRationalInit(1001, 60000); // 1501.5/90000
        m_totalDuration = av_add_q(m_totalDuration, pkt_dur);
    }

    m_justAfterChange = false;

    if (m_exitafterdecoded)
        m_gotVideoFrame = true;

    return true;
}

// Maximum retries - 500 = 5 seconds
#define PACKET_MAX_RETRIES 5000
#define RETRY_WAIT_TIME 10000   // microseconds
bool AvFormatDecoder::ProcessVideoPacket(AVStream *curstream, AVPacket *pkt)
{
    int retryCount = 0;
    int gotpicture = 0;
    AVCodecContext *context = gCodecMap->getCodecContext(curstream);
    MythAVFrame mpa_pic;
    if (!mpa_pic)
    {
        return false;
    }
    mpa_pic->reordered_opaque = AV_NOPTS_VALUE;

    if (pkt->pts != AV_NOPTS_VALUE)
        m_pts_detected = true;

    bool tryAgain = true;
    bool sentPacket = false;
    while (tryAgain)
    {
        int ret, ret2 = 0;
        tryAgain = false;
        gotpicture = 0;
        avcodeclock->lock();
        if (m_private_dec)
        {
            if (QString(m_ic->iformat->name).contains("avi") || !m_pts_detected)
                pkt->pts = pkt->dts;
            // TODO disallow private decoders for dvd playback
            // N.B. we do not reparse the frame as it breaks playback for
            // everything but libmpeg2
            ret = m_private_dec->GetFrame(curstream, mpa_pic, &gotpicture, pkt);
            sentPacket = true;
        }
        else
        {
            if (!m_use_frame_timing)
                context->reordered_opaque = pkt->pts;

            //  SUGGESTION
            //  Now that avcodec_decode_video2 is deprecated and replaced
            //  by 2 calls (receive frame and send packet), this could be optimized
            //  into separate routines or separate threads.
            //  Also now that it always consumes a whole buffer some code
            //  in the caller may be able to be optimized.

            // FilteredReceiveFrame will call avcodec_receive_frame and
            // apply any codec-dependent filtering
            ret = m_mythcodecctx->FilteredReceiveFrame(context, mpa_pic);

            if (ret == 0)
                gotpicture = 1;
            else
                gotpicture = 0;
            if (ret == AVERROR(EAGAIN))
                ret = 0;
            // If we got a picture do not send the packet until we have
            // all available pictures
            if (ret==0 && !gotpicture)
            {
                ret2 = avcodec_send_packet(context, pkt);
                if (ret2 == AVERROR(EAGAIN))
                {
                    tryAgain = true;
                    ret2 = 0;
                }
                else
                {
                    sentPacket = true;
                }
            }
        }
        avcodeclock->unlock();

        if (ret < 0 || ret2 < 0)
        {
            char error[AV_ERROR_MAX_STRING_SIZE];
            if (ret < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("video avcodec_receive_frame error: %1 (%2) gotpicture:%3")
                    .arg(av_make_error_string(error, sizeof(error), ret))
                    .arg(ret).arg(gotpicture));
            }
            if (ret2 < 0)
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("video avcodec_send_packet error: %1 (%2) gotpicture:%3")
                    .arg(av_make_error_string(error, sizeof(error), ret2))
                    .arg(ret2).arg(gotpicture));
            if (++m_averror_count > SEQ_PKT_ERR_MAX)
            {
                // If erroring on GPU assist, try switching to software decode
                if (codec_is_std(m_video_codec_id))
                    m_parent->SetErrored(QObject::tr("Video Decode Error"));
                else
                    m_streams_changed = true;
            }
            if (ret == AVERROR_EXTERNAL || ret2 == AVERROR_EXTERNAL)
                m_streams_changed = true;
            return false;
        }

        if (tryAgain)
        {
            if (++retryCount > PACKET_MAX_RETRIES)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("ERROR: Video decode buffering retries exceeded maximum"));
                return false;
            }
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Video decode buffering retry"));
            usleep(RETRY_WAIT_TIME);
        }
    }
    // averror_count counts sequential errors, so if you have a successful
    // packet then reset it
    m_averror_count = 0;
    if (gotpicture)
    {
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("video timecodes packet-pts:%1 frame-pts:%2 packet-dts: %3 frame-dts:%4")
                .arg(pkt->pts).arg(mpa_pic->pts).arg(pkt->pts)
                .arg(mpa_pic->pkt_dts));

        if (!m_use_frame_timing)
        {
            int64_t pts = 0;

            // Detect faulty video timestamps using logic from ffplay.
            if (pkt->dts != AV_NOPTS_VALUE)
            {
                m_faulty_dts += (pkt->dts <= m_last_dts_for_fault_detection);
                m_last_dts_for_fault_detection = pkt->dts;
            }
            if (mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
            {
                m_faulty_pts += (mpa_pic->reordered_opaque <= m_last_pts_for_fault_detection);
                m_last_pts_for_fault_detection = mpa_pic->reordered_opaque;
                m_reordered_pts_detected = true;
            }

            // Explicity use DTS for DVD since they should always be valid for every
            // frame and fixups aren't enabled for DVD.
            // Select reordered_opaque (PTS) timestamps if they are less faulty or the
            // the DTS timestamp is missing. Also use fixups for missing PTS instead of
            // DTS to avoid oscillating between PTS and DTS. Only select DTS if PTS is
            // more faulty or never detected.
            if (m_force_dts_timestamps || ringBuffer->IsDVD())
            {
                if (pkt->dts != AV_NOPTS_VALUE)
                    pts = pkt->dts;
                m_pts_selected = false;
            }
            else if (m_private_dec && m_private_dec->NeedsReorderedPTS() &&
                    mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
            {
                pts = mpa_pic->reordered_opaque;
                m_pts_selected = true;
            }
            else if (m_faulty_pts <= m_faulty_dts && m_reordered_pts_detected)
            {
                if (mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
                    pts = mpa_pic->reordered_opaque;
                m_pts_selected = true;
            }
            else if (pkt->dts != AV_NOPTS_VALUE)
            {
                pts = pkt->dts;
                m_pts_selected = false;
            }

            LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_DEBUG, LOC +
                QString("video packet timestamps reordered %1 pts %2 dts %3 (%4)")
                    .arg(mpa_pic->reordered_opaque).arg(pkt->pts).arg(pkt->dts)
                    .arg((m_force_dts_timestamps) ? "dts forced" :
                        (m_pts_selected) ? "reordered" : "dts"));

            mpa_pic->reordered_opaque = pts;
        }
        ProcessVideoFrame(curstream, mpa_pic);
    }
    if (!sentPacket)
    {
        // MythTV logic expects that only one frame is processed
        // Save the packet for later and return.
        AVPacket *newPkt = new AVPacket;
        memset(newPkt, 0, sizeof(AVPacket));
        av_init_packet(newPkt);
        av_packet_ref(newPkt, pkt);
        m_storedPackets.prepend(newPkt);
    }
    return true;
}

bool AvFormatDecoder::ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic)
{

    AVCodecContext *context = gCodecMap->getCodecContext(stream);

    // We need to mediate between ATSC and SCTE data when both are present.  If
    // both are present, we generally want to prefer ATSC.  However, there may
    // be large sections of the recording where ATSC is used and other sections
    // where SCTE is used.  In that case, we want to allow a natural transition
    // from ATSC back to SCTE.  We do this by allowing 10 consecutive SCTE
    // frames, without an intervening ATSC frame, to cause a switch back to
    // considering SCTE frames.  The number 10 is somewhat arbitrarily chosen.

    uint cc_len = (uint) max(mpa_pic->scte_cc_len,0);
    uint8_t *cc_buf = mpa_pic->scte_cc_buf;
    bool scte = true;

    // If we saw SCTE, then decrement a nonzero ignore_scte count.
    if (cc_len > 0 && m_ignore_scte)
        --m_ignore_scte;

    // If both ATSC and SCTE caption data are available, prefer ATSC
    if ((mpa_pic->atsc_cc_len > 0) || m_ignore_scte)
    {
        cc_len = (uint) max(mpa_pic->atsc_cc_len, 0);
        cc_buf = mpa_pic->atsc_cc_buf;
        scte = false;
        // If we explicitly saw ATSC, then reset ignore_scte count.
        if (cc_len > 0)
            m_ignore_scte = 10;
    }

    // Decode CEA-608 and CEA-708 captions
    for (uint i = 0; i < cc_len; i += ((cc_buf[i] & 0x1f) * 3) + 2)
        DecodeDTVCC(cc_buf + i, cc_len - i, scte);

    if (cc_len == 0) {
        // look for A53 captions
        AVFrameSideData *side_data = av_frame_get_side_data(mpa_pic, AV_FRAME_DATA_A53_CC);
        if (side_data && (side_data->size > 0)) {
            DecodeCCx08(side_data->data, side_data->size, false);
        }
    }

    VideoFrame *picframe = (VideoFrame *)(mpa_pic->opaque);

    if (FlagIsSet(kDecodeNoDecode))
    {
        // Do nothing, we just want the pts, captions, subtites, etc.
        // So we can release the unconverted blank video frame to the
        // display queue.
    }
    else if (!m_directrendering)
    {
        AVFrame *tmp_frame = nullptr;
        AVFrame *use_frame = nullptr;
        VideoFrame *xf = picframe;
        picframe = m_parent->GetNextVideoFrame();
        unsigned char *buf = picframe->buf;
        bool used_picframe=false;
#if defined(USING_VAAPI2) || defined(USING_NVDEC)
        if (AV_PIX_FMT_CUDA == (AVPixelFormat)mpa_pic->format
            || IS_VAAPI_PIX_FMT((AVPixelFormat)mpa_pic->format))
        {
            int ret = 0;
            tmp_frame = av_frame_alloc();
            use_frame = tmp_frame;
            /* retrieve data from GPU to CPU */
            AVPixelFormat *pixelformats = nullptr;
            ret = av_hwframe_transfer_get_formats(mpa_pic->hw_frames_ctx,
                AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                &pixelformats, 0);
            if (ret==0)
            {
                for (AVPixelFormat *format = pixelformats; *format != AV_PIX_FMT_NONE; format++)
                {
                    if (*format == AV_PIX_FMT_YUV420P)
                    {
                        // Retrieve the picture directly into the Video Frame Buffer
                        used_picframe = true;
                        use_frame->format = AV_PIX_FMT_YUV420P;
                        for (int i = 0; i < 3; i++)
                        {
                            use_frame->data[i]     = buf + picframe->offsets[i];
                            use_frame->linesize[i] = picframe->pitches[i];
                        }
                        // Dummy release method - we do not want to free the buffer
                        AVBufferRef *buffer =
                            av_buffer_create((uint8_t*)picframe, 0, dummy_release_avf_buffer, this, 0);
                        use_frame->buf[0] = buffer;
                        use_frame->width = mpa_pic->width;
                        use_frame->height = mpa_pic->height;
                        break;
                    }
                }
            }
            if ((ret = av_hwframe_transfer_data(use_frame, mpa_pic, 0)) < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC
                    + QString("Error %1 transferring the data to system memory")
                        .arg(ret));
                av_frame_free(&use_frame);
                return false;
            }
            av_freep(&pixelformats);
        }
        else
#endif // USING_VAAPI2 || USING_NVDEC
            use_frame = mpa_pic;

        if (!used_picframe)
        {
            AVFrame tmppicture;
            av_image_fill_arrays(tmppicture.data, tmppicture.linesize,
                buf, AV_PIX_FMT_YUV420P, use_frame->width,
                        use_frame->height, IMAGE_ALIGN);
            tmppicture.data[0] = buf + picframe->offsets[0];
            tmppicture.data[1] = buf + picframe->offsets[1];
            tmppicture.data[2] = buf + picframe->offsets[2];
            tmppicture.linesize[0] = picframe->pitches[0];
            tmppicture.linesize[1] = picframe->pitches[1];
            tmppicture.linesize[2] = picframe->pitches[2];

            QSize dim = get_video_dim(*context);
            m_sws_ctx = sws_getCachedContext(m_sws_ctx, use_frame->width,
                                        use_frame->height, (AVPixelFormat)use_frame->format,
                                        use_frame->width, use_frame->height,
                                        AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                        nullptr, nullptr, nullptr);
            if (!m_sws_ctx)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate sws context");
                return false;
            }
            sws_scale(m_sws_ctx, use_frame->data, use_frame->linesize, 0, dim.height(),
                    tmppicture.data, tmppicture.linesize);
        }
        if (xf)
        {
            // Set the frame flags, but then discard it
            // since we are not using it for display.
            xf->interlaced_frame = mpa_pic->interlaced_frame;
            xf->top_field_first = mpa_pic->top_field_first;
            xf->frameNumber = m_framesPlayed;
            xf->aspect = m_current_aspect;
            m_parent->DiscardVideoFrame(xf);
        }
        if (tmp_frame)
            av_frame_free(&tmp_frame);
    }
    else if (!picframe)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "NULL videoframe - direct rendering not"
                                       "correctly initialized.");
        return false;
    }

    long long pts;
    if (m_use_frame_timing)
    {
        pts = mpa_pic->pts;
        if (pts == AV_NOPTS_VALUE)
            pts = mpa_pic->pkt_dts;
        if (pts == AV_NOPTS_VALUE)
            pts = mpa_pic->reordered_opaque;
        if (pts == AV_NOPTS_VALUE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No PTS found - unable to process video.");
            return false;
        }
        pts = (long long)(av_q2d(stream->time_base) *
                                pts * 1000);
    }
    else
        pts = (long long)(av_q2d(stream->time_base) *
                                mpa_pic->reordered_opaque * 1000);

    long long temppts = pts;
    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!ringBuffer->IsDVD() &&
        temppts <= m_lastvpts &&
        (temppts + (1000 / m_fps) > m_lastvpts || temppts <= 0))
    {
        temppts = m_lastvpts;
        temppts += (long long)(1000 / m_fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += (long long)(mpa_pic->repeat_pict * 500 / m_fps);
    }

    // Calculate actual fps from the pts values.
    long long ptsdiff = temppts - m_lastvpts;
    double calcfps = 1000.0 / ptsdiff;
    if (calcfps < 121.0 && calcfps > 3.0)
    {
        // If fps has doubled due to frame-doubling deinterlace
        // Set fps to double value.
        double fpschange = calcfps / static_cast<double>(m_fps);
        int prior = m_fpsMultiplier;
        if (fpschange > 1.9 && fpschange < 2.1)
            m_fpsMultiplier = 2;
        if (fpschange > 0.9 && fpschange < 1.1)
            m_fpsMultiplier = 1;
        if (m_fpsMultiplier != prior)
            m_parent->SetFrameRate(m_fps);
    }

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
        QString("video timecode %1 %2 %3 %4%5")
            .arg(m_use_frame_timing ? mpa_pic->pts : mpa_pic->reordered_opaque).arg(pts)
            .arg(temppts).arg(m_lastvpts)
            .arg((pts != temppts) ? " fixup" : ""));

    if (picframe)
    {
        picframe->interlaced_frame = mpa_pic->interlaced_frame;
        picframe->top_field_first  = mpa_pic->top_field_first;
        picframe->repeat_pict      = mpa_pic->repeat_pict;
        picframe->disp_timecode    = NormalizeVideoTimecode(stream, temppts);
        picframe->frameNumber      = m_framesPlayed;
        picframe->aspect           = m_current_aspect;
        picframe->dummy            = 0;
        picframe->directrendering  = m_directrendering ? 1 : 0;

        m_parent->ReleaseNextVideoFrame(picframe, temppts);
    }

    m_decoded_video_frame = picframe;
    m_gotVideoFrame = true;
    if (++m_fpsSkip >= m_fpsMultiplier)
    {
        ++m_framesPlayed;
        m_fpsSkip = 0;
    }

    m_lastvpts = temppts;
    if (!m_firstvpts && m_firstvptsinuse)
        m_firstvpts = temppts;

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
    unsigned long long utc = m_lastccptsu;

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
                if (m_tracks[kTrackTypeTeletextMenu].empty())
                {
                    StreamInfo si(pkt->stream_index, 0, 0, 0, 0);
                    m_tracks[kTrackTypeTeletextMenu].push_back(si);
                }
                m_ttd->Decode(buf+1, VBI_IVTV);
                break;
            case VBI_TYPE_CC:
                // PAL   line 22 (rare)
                // NTSC  line 21
                if (21 == line)
                {
                    int data = (buf[2] << 8) | buf[1];
                    if (cc608_good_parity(m_cc608_parity_table, data))
                        m_ccd608->FormatCCField(utc/1000, field, data);
                    utc += 33367;
                }
                break;
            case VBI_TYPE_VPS: // Video Programming System
                // PAL   line 16
                m_ccd608->DecodeVPS(buf+1); // a.k.a. PDC
                break;
            case VBI_TYPE_WSS: // Wide Screen Signal
                // PAL   line 23
                // NTSC  line 20
                m_ccd608->DecodeWSS(buf+1);
                break;
        }
        buf += 43;
    }
    m_lastccptsu = utc;
    UpdateCaptionTracksFromStreams(true, false);
}

/** \fn AvFormatDecoder::ProcessDVBDataPacket(const AVStream*, const AVPacket*)
 *  \brief Process DVB Teletext.
 *  \sa TeletextDecoder
 */
void AvFormatDecoder::ProcessDVBDataPacket(
    const AVStream* /*stream*/, const AVPacket *pkt)
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
                m_ttd->Decode(buf, VBI_DVB);
            buf += 42;
        }
        else if (*buf == 0x03)
        {
            buf += 4;
            if ((buf_end - buf) >= 42)
                m_ttd->Decode(buf, VBI_DVB_SUBTITLE);
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
    if (!m_itv && ! (m_itv = m_parent->GetInteractiveTV()))
        return;

    // The packet may contain several tables.
    uint8_t *data = pkt->data;
    int length = pkt->size;
    int componentTag, dataBroadcastId;
    unsigned carouselId;
    {
        QMutexLocker locker(avcodeclock);
        componentTag    = str->component_tag;
        dataBroadcastId = str->data_id;
        carouselId  = (unsigned) str->carousel_id;
    }
    while (length > 3)
    {
        uint16_t sectionLen = (((data[1] & 0xF) << 8) | data[2]) + 3;

        if (sectionLen > length) // This may well be filler
            return;

        m_itv->ProcessDSMCCSection(data, sectionLen,
                                 componentTag, carouselId,
                                 dataBroadcastId);
        length -= sectionLen;
        data += sectionLen;
    }
#else
    Q_UNUSED(str);
    Q_UNUSED(pkt);
#endif // USING_MHEG
}

bool AvFormatDecoder::ProcessSubtitlePacket(AVStream *curstream, AVPacket *pkt)
{
    if (!m_parent->GetSubReader(pkt->stream_index))
        return true;

    long long pts = 0;

    if (pkt->dts != AV_NOPTS_VALUE)
        pts = (long long)(av_q2d(curstream->time_base) * pkt->dts * 1000);

    avcodeclock->lock();
    int subIdx = m_selectedTrack[kTrackTypeSubtitle].m_av_stream_index;
    bool isForcedTrack = m_selectedTrack[kTrackTypeSubtitle].m_forced;
    avcodeclock->unlock();

    int gotSubtitles = 0;
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(AVSubtitle));

    if (ringBuffer->IsDVD())
    {
        if (ringBuffer->DVD()->NumMenuButtons() > 0)
        {
            ringBuffer->DVD()->GetMenuSPUPkt(pkt->data, pkt->size,
                                             curstream->id, pts);
        }
        else
        {
            if (pkt->stream_index == subIdx)
            {
                QMutexLocker locker(avcodeclock);
                ringBuffer->DVD()->DecodeSubtitles(&subtitle, &gotSubtitles,
                                                   pkt->data, pkt->size, pts);
            }
        }
    }
    else if (m_decodeAllSubtitles || pkt->stream_index == subIdx)
    {
        AVCodecContext *ctx = gCodecMap->getCodecContext(curstream);
        QMutexLocker locker(avcodeclock);
        avcodec_decode_subtitle2(ctx, &subtitle, &gotSubtitles,
            pkt);

        subtitle.start_display_time += pts;
        subtitle.end_display_time += pts;
    }

    if (gotSubtitles)
    {
        if (isForcedTrack)
            subtitle.forced = static_cast<int>(true);
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("subtl timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts)
                .arg(subtitle.start_display_time)
                .arg(subtitle.end_display_time));

        bool forcedon = m_parent->GetSubReader(pkt->stream_index)->AddAVSubtitle(
                subtitle, curstream->codecpar->codec_id == AV_CODEC_ID_XSUB,
                m_parent->GetAllowForcedSubtitles());
         m_parent->EnableForcedSubtitles(forcedon || isForcedTrack);
    }

    return true;
}

bool AvFormatDecoder::ProcessRawTextPacket(AVPacket *pkt)
{
    if (!(m_decodeAllSubtitles ||
        m_selectedTrack[kTrackTypeRawText].m_av_stream_index == pkt->stream_index))
    {
        return false;
    }

    if (!m_parent->GetSubReader(pkt->stream_index+0x2000))
        return false;

    QTextCodec *codec = QTextCodec::codecForName("utf-8");
    QTextDecoder *dec = codec->makeDecoder();
    QString text      = dec->toUnicode((const char*)pkt->data, pkt->size - 1);
    QStringList list  = text.split('\n', QString::SkipEmptyParts);
    delete dec;

    m_parent->GetSubReader(pkt->stream_index+0x2000)->
        AddRawTextSubtitle(list, pkt->duration);

    return true;
}

bool AvFormatDecoder::ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                        DecodeType decodetype)
{
    enum AVCodecID codec_id = curstream->codecpar->codec_id;

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
                m_allowedquit |= (m_itv && m_itv->ImageHasChanged());
#else
            Q_UNUSED(decodetype);
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
    int ret = DecoderBase::SetTrack(type, trackNo);

    if (kTrackTypeAudio == type)
    {
        QString msg = SetupAudioStream() ? "" : "not ";
        LOG(VB_AUDIO, LOG_INFO, LOC + "Audio stream type "+msg+"changed.");
    }

    return ret;
}

QString AvFormatDecoder::GetTrackDesc(uint type, uint trackNo) const
{
    if (!m_ic || trackNo >= m_tracks[type].size())
        return "";

    bool forced = m_tracks[type][trackNo].m_forced;
    int lang_key = m_tracks[type][trackNo].m_language;
    QString forcedString = forced ? QObject::tr(" (forced)") : "";
    if (kTrackTypeAudio == type)
    {
        QString msg = iso639_key_toName(lang_key);

        switch (m_tracks[type][trackNo].m_audio_type)
        {
            case kAudioTypeNormal :
            {
                int av_index = m_tracks[kTrackTypeAudio][trackNo].m_av_stream_index;
                AVStream *s = m_ic->streams[av_index];

                if (s)
                {
                    AVCodecParameters *par = s->codecpar;
                    AVCodecContext *ctx = gCodecMap->getCodecContext(s);
                    if (par->codec_id == AV_CODEC_ID_MP3)
                        msg += QString(" MP3");
                    else if (ctx && ctx->codec)
                        msg += QString(" %1").arg(ctx->codec->name).toUpper();

                    int channels = 0;
                    if (ringBuffer->IsDVD() || par->channels)
                        channels = m_tracks[kTrackTypeAudio][trackNo].m_orig_num_channels;

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
                            .arg(toString(m_tracks[type][trackNo].m_audio_type));
                break;
        }

        return QString("%1: %2").arg(trackNo + 1).arg(msg);
    }
    if (kTrackTypeSubtitle == type)
    {
        return QObject::tr("Subtitle") + QString(" %1: %2%3")
            .arg(trackNo + 1).arg(iso639_key_toName(lang_key))
            .arg(forcedString);
    }
    if (forced && kTrackTypeRawText == type)
    {
        return DecoderBase::GetTrackDesc(type, trackNo) + forcedString;
    }
    return DecoderBase::GetTrackDesc(type, trackNo);
}

int AvFormatDecoder::GetTeletextDecoderType(void) const
{
    return m_ttd->GetDecoderType();
}

QString AvFormatDecoder::GetXDS(const QString &key) const
{
    return m_ccd608->GetXDS(key);
}

QByteArray AvFormatDecoder::GetSubHeader(uint trackNo) const
{
    if (trackNo >= m_tracks[kTrackTypeSubtitle].size())
        return QByteArray();

    int index = m_tracks[kTrackTypeSubtitle][trackNo].m_av_stream_index;
    AVCodecContext *ctx = gCodecMap->getCodecContext(m_ic->streams[index]);
    if (!ctx)
        return QByteArray();

    return QByteArray((char *)ctx->subtitle_header,
                      ctx->subtitle_header_size);
}

void AvFormatDecoder::GetAttachmentData(uint trackNo, QByteArray &filename,
                                        QByteArray &data)
{
    if (trackNo >= m_tracks[kTrackTypeAttachment].size())
        return;

    int index = m_tracks[kTrackTypeAttachment][trackNo].m_av_stream_index;
    AVDictionaryEntry *tag = av_dict_get(m_ic->streams[index]->metadata,
                                         "filename", nullptr, 0);
    if (tag)
        filename  = QByteArray(tag->value);
    AVCodecParameters *par = m_ic->streams[index]->codecpar;
    data = QByteArray((char *)par->extradata, par->extradata_size);
}

bool AvFormatDecoder::SetAudioByComponentTag(int tag)
{
    for (size_t i = 0; i < m_tracks[kTrackTypeAudio].size(); i++)
    {
        AVStream *s  = m_ic->streams[m_tracks[kTrackTypeAudio][i].m_av_stream_index];
        if (s)
        {
            if ((s->component_tag == tag) ||
                ((tag <= 0) && s->component_tag <= 0))
            {
                return SetTrack(kTrackTypeAudio, i) != -1;
            }
        }
    }
    return false;
}

bool AvFormatDecoder::SetVideoByComponentTag(int tag)
{
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *s  = m_ic->streams[i];
        if (s)
        {
            if (s->component_tag == tag)
            {
                StreamInfo si(i, 0, 0, 0, 0);
                m_selectedTrack[kTrackTypeVideo] = si;
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
                               const vector<int> &ftype)
{
    vector<int> ret;

    vector<int>::const_iterator it = ftype.begin();
    for (; it != ftype.end(); ++it)
    {
        if ((lang_key < 0) || tracks[*it].m_language == lang_key)
            ret.push_back(*it);
    }

    return ret;
}

static vector<int> filter_type(const sinfo_vec_t &tracks, AudioTrackType type)
{
    vector<int> ret;

    for (size_t i = 0; i < tracks.size(); i++)
    {
        if (tracks[i].m_audio_type == type)
            ret.push_back(i);
    }

    return ret;
}

int AvFormatDecoder::filter_max_ch(const AVFormatContext *ic,
                                   const sinfo_vec_t     &tracks,
                                   const vector<int>     &fs,
                                   enum AVCodecID         codecId,
                                   int                    profile)
{
    int selectedTrack = -1, max_seen = -1;

    vector<int>::const_iterator it = fs.begin();
    for (; it != fs.end(); ++it)
    {
        const int stream_index = tracks[*it].m_av_stream_index;
        AVCodecParameters *par = ic->streams[stream_index]->codecpar;
        if ((codecId == AV_CODEC_ID_NONE || codecId == par->codec_id) &&
            (max_seen < par->channels))
        {
            if (codecId == AV_CODEC_ID_DTS && profile > 0)
            {
                // we cannot decode dts-hd, so only select it if passthrough
                if (!DoPassThrough(par, true) || par->profile != profile)
                    continue;
            }
            selectedTrack = *it;
            max_seen = par->channels;
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
    const sinfo_vec_t &atracks = m_tracks[kTrackTypeAudio];
    StreamInfo        &wtrack  = m_wantedTrack[kTrackTypeAudio];
    StreamInfo        &strack  = m_selectedTrack[kTrackTypeAudio];
    int               &ctrack  = m_currentTrack[kTrackTypeAudio];

    uint numStreams = atracks.size();
    if ((ctrack >= 0) && (ctrack < (int)numStreams))
        return ctrack; // audio already selected

#if 0
    // enable this to print streams
    for (uint i = 0; i < atracks.size(); i++)
    {
        int idx = atracks[i].av_stream_index;
        AVCodecContext *codec_ctx = m_ic->streams[idx]->codec;
        AudioInfo item(codec_ctx->codec_id, codec_ctx->bps,
                       codec_ctx->sample_rate, codec_ctx->channels,
                       DoPassThrough(codec_ctx, true));
        LOG(VB_AUDIO, LOG_DEBUG, LOC + " * " + item.toString());
    }
#endif

    int selTrack = (1 == numStreams) ? 0 : -1;
    int wlang    = wtrack.m_language;


    if ((selTrack < 0) && (wtrack.m_av_substream_index >= 0))
    {
        LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to reselect audio sub-stream");
        // Dual stream without language information: choose
        // the previous substream that was kept in wtrack,
        // ignoring the stream index (which might have changed).
        int substream_index = wtrack.m_av_substream_index;

        for (uint i = 0; i < numStreams; i++)
        {
            if (atracks[i].m_av_substream_index == substream_index)
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
        uint windx = wtrack.m_language_index;
        for (uint i = 0; i < numStreams; i++)
        {
            if (wlang == atracks[i].m_language)
            {
                selTrack = i;

                if (windx == atracks[i].m_language_index)
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
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                     FF_PROFILE_DTS_HD_MA);
        if (selTrack < 0)
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_TRUEHD);

        if (selTrack < 0 && m_audio->CanDTSHD())
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                     FF_PROFILE_DTS_HD_HRA);
        if (selTrack < 0)
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_EAC3);

        if (selTrack < 0)
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS);

        if (selTrack < 0)
            selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_AC3);

        if (selTrack < 0)
            selTrack = filter_max_ch(m_ic, atracks, flang);

        // try to get best track for most preferred language
        // Set by the "Guide Data" language prefs in Appearance.
        if (selTrack < 0)
        {
            vector<int>::const_iterator it = m_languagePreference.begin();
            for (; it !=  m_languagePreference.end() && selTrack < 0; ++it)
            {
                flang = filter_lang(atracks, *it, ftype);

                if (m_audio->CanDTSHD())
                    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                             FF_PROFILE_DTS_HD_MA);
                if (selTrack < 0)
                    selTrack = filter_max_ch(m_ic, atracks, flang,
                                             AV_CODEC_ID_TRUEHD);

                if (selTrack < 0 && m_audio->CanDTSHD())
                    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                             FF_PROFILE_DTS_HD_HRA);

                if (selTrack < 0)
                    selTrack = filter_max_ch(m_ic, atracks, flang,
                                             AV_CODEC_ID_EAC3);

                if (selTrack < 0)
                    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS);

                if (selTrack < 0)
                    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_AC3);

                if (selTrack < 0)
                    selTrack = filter_max_ch(m_ic, atracks, flang);
            }
        }

        // could not select track based on user preferences (language)
        // try to select the default track
        if (selTrack < 0 && numStreams)
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to select default track");
            for (size_t i = 0; i < atracks.size(); i++) {
                int idx = atracks[i].m_av_stream_index;
                if (m_ic->streams[idx]->disposition & AV_DISPOSITION_DEFAULT)
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
            flang = filter_lang(atracks, -1, ftype);

            if (m_audio->CanDTSHD())
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                         FF_PROFILE_DTS_HD_MA);
            if (selTrack < 0)
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_TRUEHD);

            if (selTrack < 0 && m_audio->CanDTSHD())
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                         FF_PROFILE_DTS_HD_HRA);

            if (selTrack < 0)
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_EAC3);

            if (selTrack < 0)
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS);

            if (selTrack < 0)
                selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_AC3);

            if (selTrack < 0)
                selTrack = filter_max_ch(m_ic, atracks, flang);
        }
    }

    if (selTrack < 0)
    {
        strack.m_av_stream_index = -1;
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

        if (wtrack.m_av_stream_index < 0)
            wtrack = strack;

        LOG(VB_AUDIO, LOG_INFO, LOC +
            QString("Selected track %1 (A/V Stream #%2)")
                .arg(GetTrackDesc(kTrackTypeAudio, ctrack))
                .arg(strack.m_av_stream_index));
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
    AVCodecContext *ctx = gCodecMap->getCodecContext(curstream);
    int ret             = 0;
    int data_size       = 0;
    bool firstloop      = true;
    int decoded_size    = -1;

    avcodeclock->lock();
    int audIdx = m_selectedTrack[kTrackTypeAudio].m_av_stream_index;
    int audSubIdx = m_selectedTrack[kTrackTypeAudio].m_av_substream_index;
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
        bool isDual = ctx->avcodec_dual_language != 0;
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
            QMutexLocker locker(avcodeclock);

            if (DoPassThrough(curstream->codecpar, false) || !DecoderWillDownmix(ctx))
            {
                // for passthru or codecs for which the decoder won't downmix
                // let the decoder set the number of channels. For other codecs
                // we downmix if necessary in audiooutputbase
                ctx->request_channel_layout = 0;
            }
            else // No passthru, the decoder will downmix
            {
                ctx->request_channel_layout =
                    av_get_default_channel_layout(m_audio->GetMaxChannels());
                if (ctx->codec_id == AV_CODEC_ID_AC3)
                    ctx->channels = m_audio->GetMaxChannels();
            }

            ret = m_audio->DecodeAudio(ctx, m_audioSamples, data_size, &tmp_pkt);
            decoded_size = data_size;
            already_decoded = true;
            reselectAudioTrack |= ctx->channels;
        }

        if (reselectAudioTrack)
        {
            QMutexLocker locker(avcodeclock);
            m_currentTrack[kTrackTypeAudio] = -1;
            m_selectedTrack[kTrackTypeAudio].m_av_stream_index = -1;
            AutoSelectAudioTrack();
            audIdx = m_selectedTrack[kTrackTypeAudio].m_av_stream_index;
            audSubIdx = m_selectedTrack[kTrackTypeAudio].m_av_substream_index;
        }

        if (!(decodetype & kDecodeAudio) || (pkt->stream_index != audIdx)
            || !m_audio->HasAudioOut())
            break;

        if (firstloop && pkt->pts != AV_NOPTS_VALUE)
            m_lastapts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);

        if (!m_use_frame_timing)
        {
            // This code under certain conditions causes jump backwards to lose
            // audio.
            if (m_skipaudio && m_selectedTrack[kTrackTypeVideo].m_av_stream_index > -1)
            {
                if ((m_lastapts < m_lastvpts - (10.0F / m_fps)) || m_lastvpts == 0)
                    break;
                m_skipaudio = false;
            }

            // skip any audio frames preceding first video frame
            if (m_firstvptsinuse && m_firstvpts && (m_lastapts < m_firstvpts))
            {
                LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                    QString("discarding early audio timecode %1 %2 %3")
                        .arg(pkt->pts).arg(pkt->dts).arg(m_lastapts));
                break;
            }
        }
        m_firstvptsinuse = false;

        avcodeclock->lock();
        data_size = 0;

        // Check if the number of channels or sampling rate have changed
        if (ctx->sample_rate != m_audioOut.sample_rate ||
            ctx->channels    != m_audioOut.channels ||
            AudioOutputSettings::AVSampleFormatToFormat(ctx->sample_fmt,
                                                        ctx->bits_per_raw_sample) != m_audioOut.format)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Audio stream changed");
            if (ctx->channels != m_audioOut.channels)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Number of audio channels changed from %1 to %2")
                    .arg(m_audioOut.channels).arg(ctx->channels));
            }
            m_currentTrack[kTrackTypeAudio] = -1;
            m_selectedTrack[kTrackTypeAudio].m_av_stream_index = -1;
            audIdx = -1;
            AutoSelectAudioTrack();
        }

        if (m_audioOut.do_passthru)
        {
            if (!already_decoded)
            {
                if (m_audio->NeedDecodingBeforePassthrough())
                {
                    ret = m_audio->DecodeAudio(ctx, m_audioSamples, data_size, &tmp_pkt);
                    decoded_size = data_size;
                }
                else
                {
                    decoded_size = -1;
                }
            }
            memcpy(m_audioSamples, tmp_pkt.data, tmp_pkt.size);
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
                    ctx->request_channel_layout =
                        av_get_default_channel_layout(m_audio->GetMaxChannels());
                }
                else
                {
                    ctx->request_channel_layout = 0;
                }

                ret = m_audio->DecodeAudio(ctx, m_audioSamples, data_size, &tmp_pkt);
                decoded_size = data_size;
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

        long long temppts = m_lastapts;

        if (audSubIdx != -1 && !m_audioOut.do_passthru)
            extract_mono_channel(audSubIdx, &m_audioOut,
                                 (char *)m_audioSamples, data_size);

        int samplesize = AudioOutputSettings::SampleSize(m_audio->GetFormat());
        int frames = (ctx->channels <= 0 || decoded_size < 0 || !samplesize) ? -1 :
            decoded_size / (ctx->channels * samplesize);
        m_audio->AddAudioData((char *)m_audioSamples, data_size, temppts, frames);
        if (m_audioOut.do_passthru && !m_audio->NeedDecodingBeforePassthrough())
        {
            m_lastapts += m_audio->LengthLastData();
        }
        else
        {
            m_lastapts += (long long)
                ((double)(frames * 1000) / ctx->sample_rate);
        }

        LOG(VB_TIMESTAMP, LOG_INFO, LOC + QString("audio timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(m_lastapts));

        m_allowedquit |= ringBuffer->IsInStillFrame() ||
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
    AVPacket *pkt = nullptr;
    bool have_err = false;

    const DecodeType origDecodetype = decodetype;

    m_gotVideoFrame = false;

    m_frame_decoded = 0;
    m_decoded_video_frame = nullptr;

    m_allowedquit = false;
    bool storevideoframes = false;

    avcodeclock->lock();
    AutoSelectTracks();
    avcodeclock->unlock();

    m_skipaudio = (m_lastvpts == 0);

    if( !m_processFrames )
    {
        return false;
    }

    m_hasVideo = HasVideo(m_ic);
    m_needDummyVideoFrames = false;

    if (!m_hasVideo && (decodetype & kDecodeVideo))
    {
        // NB This could be an issue if the video stream is not
        // detected initially as the video buffers will be filled.
        m_needDummyVideoFrames = true;
        decodetype = (DecodeType)((int)decodetype & ~kDecodeVideo);
        m_skipaudio = false;
    }

    m_allowedquit = m_audio->IsBufferAlmostFull();

    if (m_private_dec && m_private_dec->HasBufferedFrames() &&
       (m_selectedTrack[kTrackTypeVideo].m_av_stream_index > -1))
    {
        int got_picture  = 0;
        AVStream *stream = m_ic->streams[m_selectedTrack[kTrackTypeVideo]
                                 .m_av_stream_index];
        MythAVFrame mpa_pic;
        if (!mpa_pic)
        {
            return false;
        }

        m_private_dec->GetFrame(stream, mpa_pic, &got_picture, nullptr);
        if (got_picture)
            ProcessVideoFrame(stream, mpa_pic);
    }

    while (!m_allowedquit)
    {
        if (decodetype & kDecodeAudio)
        {
            if (((m_currentTrack[kTrackTypeAudio] < 0) ||
                 (m_selectedTrack[kTrackTypeAudio].m_av_stream_index < 0)))
            {
                // disable audio request if there are no audio streams anymore
                // and we have video, otherwise allow decoding to stop
                if (m_hasVideo)
                    decodetype = (DecodeType)((int)decodetype & ~kDecodeAudio);
                else
                    m_allowedquit = true;
            }
        }
        else if ((origDecodetype & kDecodeAudio) &&
            (m_currentTrack[kTrackTypeAudio] >= 0) &&
            (m_selectedTrack[kTrackTypeAudio].m_av_stream_index >= 0))
        {
            // Turn on audio decoding again if it was on originally
            // and an audio stream has now appeared.  This can happen
            // in still DVD menus with audio
            decodetype = (DecodeType)((int)decodetype | kDecodeAudio);
        }

        StreamChangeCheck();

        if (m_gotVideoFrame)
        {
            if (decodetype == kDecodeNothing)
            {
                // no need to buffer audio or video if we
                // only care about building a keyframe map.
                // NB but allow for data only (MHEG) streams
                m_allowedquit = true;
            }
            else if ((decodetype & kDecodeAV) == kDecodeAV &&
                     (m_storedPackets.count() < max_video_queue_size) &&
                     // buffer audio to prevent audio buffer
                     // underruns in case you are setting negative values
                     // in Adjust Audio Sync.
                     m_lastapts < m_lastvpts + m_audioReadAhead &&
                     !ringBuffer->IsInStillFrame())
            {
                storevideoframes = true;
            }
            else if (decodetype & kDecodeVideo)
            {
                if (m_storedPackets.count() >= max_video_queue_size)
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Audio %1 ms behind video but already %2 "
                                "video frames queued. AV-Sync might be broken.")
                            .arg(m_lastvpts-m_lastapts).arg(m_storedPackets.count()));
                m_allowedquit = true;
                continue;
            }
        }

        if (!storevideoframes && m_storedPackets.count() > 0)
        {
            if (pkt)
            {
                av_packet_unref(pkt);
                delete pkt;
            }
            pkt = m_storedPackets.takeFirst();
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
            if (!m_ic || ((retval = ReadPacket(m_ic, pkt, storevideoframes)) < 0))
            {
                if (retval == -EAGAIN)
                    continue;

                SetEof(true);
                delete pkt;
                char errbuf[256];
                QString errmsg;
                if (av_strerror(retval, errbuf, sizeof errbuf) == 0)
                    errmsg = QString(errbuf);
                else
                    errmsg = "UNKNOWN";

                LOG(VB_GENERAL, LOG_ERR, QString("decoding error %1 (%2)")
                    .arg(errmsg).arg(retval));
                return false;
            }

            if (m_waitingForChange && pkt->pos >= m_readAdjust)
                FileChanged();

            if (pkt->pos > m_readAdjust)
                pkt->pos -= m_readAdjust;
        }

        if (!m_ic)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No context");
            av_packet_unref(pkt);
            continue;
        }

        if (pkt->stream_index >= (int)m_ic->nb_streams)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bad stream");
            av_packet_unref(pkt);
            continue;
        }

        AVStream *curstream = m_ic->streams[pkt->stream_index];

        if (!curstream)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Bad stream (NULL)");
            av_packet_unref(pkt);
            continue;
        }

        enum AVMediaType codec_type = curstream->codecpar->codec_type;

        if (storevideoframes && codec_type == AVMEDIA_TYPE_VIDEO)
        {
            // av_dup_packet(pkt);
            m_storedPackets.append(pkt);
            pkt = nullptr;
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_VIDEO &&
            pkt->stream_index == m_selectedTrack[kTrackTypeVideo].m_av_stream_index)
        {
            if (!PreProcessVideoPacket(curstream, pkt))
                continue;

            // If the resolution changed in XXXPreProcessPkt, we may
            // have a fatal error, so check for this before continuing.
            if (m_parent->IsErrored())
            {
                av_packet_unref(pkt);
                delete pkt;
                return false;
            }
        }

        if (codec_type == AVMEDIA_TYPE_SUBTITLE &&
            curstream->codecpar->codec_id == AV_CODEC_ID_TEXT)
        {
            ProcessRawTextPacket(pkt);
            av_packet_unref(pkt);
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_SUBTITLE &&
            curstream->codecpar->codec_id == AV_CODEC_ID_DVB_TELETEXT)
        {
            ProcessDVBDataPacket(curstream, pkt);
            av_packet_unref(pkt);
            continue;
        }

        if (codec_type == AVMEDIA_TYPE_DATA)
        {
            ProcessDataPacket(curstream, pkt, decodetype);
            av_packet_unref(pkt);
            continue;
        }

        AVCodecContext *ctx = gCodecMap->getCodecContext(curstream);
        if (!ctx)
        {
            if (codec_type != AVMEDIA_TYPE_VIDEO)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("No codec for stream index %1, type(%2) id(%3:%4)")
                        .arg(pkt->stream_index)
                        .arg(ff_codec_type_string(codec_type))
                        .arg(ff_codec_id_string(curstream->codecpar->codec_id))
                        .arg(curstream->codecpar->codec_id));
                // Process Stream Change in case we have no audio
                if (codec_type == AVMEDIA_TYPE_AUDIO && !m_audio->HasAudioIn())
                    m_streams_changed = true;
            }
            av_packet_unref(pkt);
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
                if (pkt->stream_index != m_selectedTrack[kTrackTypeVideo].m_av_stream_index)
                {
                    break;
                }

                if (pkt->pts != AV_NOPTS_VALUE)
                {
                    m_lastccptsu = (long long)
                                 (av_q2d(curstream->time_base)*pkt->pts*1000000);
                }

                if (!(decodetype & kDecodeVideo))
                {
                    m_framesPlayed++;
                    m_gotVideoFrame = true;
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
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Decoding - id(%1) type(%2)")
                        .arg(ff_codec_id_string(ctx->codec_id))
                        .arg(ff_codec_type_string(ctx->codec_type)));
                have_err = true;
                break;
            }
        }

        if (!have_err)
            m_frame_decoded = 1;

        av_packet_unref(pkt);
    }

    delete pkt;
    return true;
}

void AvFormatDecoder::StreamChangeCheck() {
    if (m_streams_changed)
    {
        SeekReset(0, 0, true, true);
        QMutexLocker locker(avcodeclock);
        ScanStreams(false);
        m_streams_changed=false;
    }
}

int AvFormatDecoder::ReadPacket(AVFormatContext *ctx, AVPacket *pkt, bool &/*storePacket*/)
{
    QMutexLocker locker(avcodeclock);

    return av_read_frame(ctx, pkt);
}

bool AvFormatDecoder::HasVideo(const AVFormatContext *ic)
{
    if (ic && ic->cur_pmt_sect)
    {
        const ProgramMapTable pmt(PSIPTable(ic->cur_pmt_sect));

        for (uint i = 0; i < pmt.StreamCount(); i++)
        {
            // MythTV remaps OpenCable Video to normal video during recording
            // so "dvb" is the safest choice for system info type, since this
            // will ignore other uses of the same stream id in DVB countries.
            if (pmt.IsVideo(i, "dvb"))
                return true;

            // MHEG may explicitly select a private stream as video
            if ((i == (uint)m_selectedTrack[kTrackTypeVideo].m_av_stream_index) &&
                (pmt.StreamType(i) == StreamID::PrivData))
            {
                return true;
            }
        }
    }

    return GetTrackCount(kTrackTypeVideo) != 0U;
}

bool AvFormatDecoder::GenerateDummyVideoFrames(void)
{
    while (m_needDummyVideoFrames && m_parent &&
           m_parent->GetFreeVideoFrames())
    {
        VideoFrame *frame = m_parent->GetNextVideoFrame();
        if (!frame)
            return false;

        m_parent->ClearDummyVideoFrame(frame);
        m_parent->ReleaseNextVideoFrame(frame, m_lastvpts);
        m_parent->DeLimboFrame(frame);

        frame->interlaced_frame = 0; // not interlaced
        frame->top_field_first  = 1; // top field first
        frame->repeat_pict      = 0; // not a repeated picture
        frame->frameNumber      = m_framesPlayed;
        frame->dummy            = 1;

        m_decoded_video_frame = frame;
        m_framesPlayed++;
        m_gotVideoFrame = true;
    }
    return true;
}

QString AvFormatDecoder::GetCodecDecoderName(void) const
{
    if (m_private_dec)
        return m_private_dec->GetName();
    return get_decoder_name(m_video_codec_id);
}

QString AvFormatDecoder::GetRawEncodingType(void)
{
    int stream = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    if (stream < 0 || !m_ic)
        return QString();
    return ff_codec_id_string(m_ic->streams[stream]->codecpar->codec_id);
}

void *AvFormatDecoder::GetVideoCodecPrivate(void)
{
    return nullptr; // TODO is this still needed
}

void AvFormatDecoder::SetDisablePassThrough(bool disable)
{
    if (m_selectedTrack[kTrackTypeAudio].m_av_stream_index < 0)
    {
        m_disable_passthru = disable;
        return;
    }

    if (disable != m_disable_passthru)
    {
        m_disable_passthru = disable;
        QString msg = (disable) ? "Disabling" : "Allowing";
        LOG(VB_AUDIO, LOG_INFO, LOC + msg + " pass through");

        // Force pass through state to be reanalyzed
        ForceSetupAudioStream();
    }
}

void AvFormatDecoder::ForceSetupAudioStream(void)
{
    QMutexLocker locker(avcodeclock);

    SetupAudioStream();
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

bool AvFormatDecoder::DoPassThrough(const AVCodecParameters *par, bool withProfile)
{
    bool passthru;

    // if withProfile == false, we will accept any DTS stream regardless
    // of its profile. We do so, so we can bitstream DTS-HD as DTS core
    if (!withProfile && par->codec_id == AV_CODEC_ID_DTS && !m_audio->CanDTSHD())
        passthru = m_audio->CanPassthrough(par->sample_rate, par->channels,
                                           par->codec_id, FF_PROFILE_DTS);
    else
        passthru = m_audio->CanPassthrough(par->sample_rate, par->channels,
                                           par->codec_id, par->profile);

    passthru &= !m_disable_passthru;

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
    AVStream *curstream = nullptr;
    AVCodecContext *ctx = nullptr;
    AudioInfo old_in    = m_audioIn;
    int requested_channels;

    if ((m_currentTrack[kTrackTypeAudio] >= 0) && m_ic &&
        (m_selectedTrack[kTrackTypeAudio].m_av_stream_index <=
         (int) m_ic->nb_streams) &&
        (curstream = m_ic->streams[m_selectedTrack[kTrackTypeAudio]
                                 .m_av_stream_index]) &&
        (ctx = gCodecMap->getCodecContext(curstream)))
    {
        AudioFormat fmt =
            AudioOutputSettings::AVSampleFormatToFormat(ctx->sample_fmt,
                                                        ctx->bits_per_raw_sample);

        if (av_sample_fmt_is_planar(ctx->sample_fmt))
        {
            LOG(VB_AUDIO, LOG_INFO, LOC + QString("Audio data is planar"));
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

        bool using_passthru = DoPassThrough(curstream->codecpar, false);

        requested_channels = ctx->channels;
        ctx->request_channel_layout =
            av_get_default_channel_layout(requested_channels);

        if (!using_passthru &&
            ctx->channels > (int)m_audio->GetMaxChannels() &&
            DecoderWillDownmix(ctx))
        {
            requested_channels = m_audio->GetMaxChannels();
            ctx->request_channel_layout =
                av_get_default_channel_layout(requested_channels);
        }
        else
        {
            ctx->request_channel_layout = 0;
        }

        info = AudioInfo(ctx->codec_id, fmt, ctx->sample_rate,
                         requested_channels, using_passthru, ctx->channels,
                         ctx->codec_id == AV_CODEC_ID_DTS ? ctx->profile : 0);
    }

    if (!ctx)
    {
        if (GetTrackCount(kTrackTypeAudio))
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "No codec context. Returning false");
        return false;
    }

    if (info == m_audioIn)
        return false;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Initializing audio parms from " +
        QString("audio track #%1").arg(m_currentTrack[kTrackTypeAudio]+1));

    m_audioOut = m_audioIn = info;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Audio format changed " +
        QString("\n\t\t\tfrom %1 to %2")
            .arg(old_in.toString()).arg(m_audioOut.toString()));

    m_audio->SetAudioParams(m_audioOut.format, ctx->channels,
                            requested_channels,
                            m_audioOut.codec_id, m_audioOut.sample_rate,
                            m_audioOut.do_passthru, m_audioOut.codec_profile);
    m_audio->ReinitAudio();
    AudioOutput *audioOutput = m_audio->GetAudioOutput();
    if (audioOutput)
        audioOutput->SetSourceBitrate(ctx->bit_rate);

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

        if (m_audioOut.do_passthru)
            lcd->setVariousLEDs(VARIOUS_SPDIF, true);
        else
            lcd->setVariousLEDs(VARIOUS_SPDIF, false);

        switch (m_audioIn.channels)
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
    int64_t start_time, end_time;
    int64_t duration;
    AVStream *st = nullptr;

    start_time = INT64_MAX;
    end_time = INT64_MIN;

    for (uint i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st1 = ic->streams[i];
        if (st1 && st1->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            st = st1;
            break;
        }
    }
    if (!st)
        return;

   duration = INT64_MIN;
   if (st->start_time != AV_NOPTS_VALUE && st->time_base.den) {
       int64_t start_time1= av_rescale_q(st->start_time, st->time_base, AV_TIME_BASE_Q);
       if (start_time1 < start_time)
           start_time = start_time1;
       if (st->duration != AV_NOPTS_VALUE) {
           int64_t end_time1 = start_time1 +
                      av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
           if (end_time1 > end_time)
               end_time = end_time1;
       }
   }
   if (st->duration != AV_NOPTS_VALUE) {
       int64_t duration1 = av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
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
        int64_t filesize;
        ic->duration = duration;
        if (ic->pb && (filesize = avio_size(ic->pb)) > 0) {
            /* compute the bitrate */
            ic->bit_rate = (double)filesize * 8.0 * AV_TIME_BASE /
                (double)ic->duration;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
