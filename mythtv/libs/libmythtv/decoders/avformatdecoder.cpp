#include "avformatdecoder.h"

#include <unistd.h>

// C++ headers
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/error.h"
#include "libavutil/intreadwrite.h" // for AV_RB32 and AV_RB24
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#include "libavutil/stereo3d.h"
#include "libavutil/imgutils.h"
#include "libavutil/display.h"
}

#if CONFIG_MEDIACODEC // Android
extern "C" {
#include "libavcodec/jni.h"
}
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QJniEnvironment>
#define QAndroidJniEnvironment QJniEnvironment
#endif
#endif // Android

// regardless of building with V4L2 or not, enable IVTV VBI data
// from <linux/videodev2.h> under SPDX-License-Identifier: ((GPL-2.0+ WITH Linux-syscall-note) OR BSD-3-Clause)
/*
 * V4L2_MPEG_STREAM_VBI_FMT_IVTV:
 *
 * Structure of payload contained in an MPEG 2 Private Stream 1 PES Packet in an
 * MPEG-2 Program Pack that contains V4L2_MPEG_STREAM_VBI_FMT_IVTV Sliced VBI
 * data
 *
 * Note, the MPEG-2 Program Pack and Private Stream 1 PES packet header
 * definitions are not included here.  See the MPEG-2 specifications for details
 * on these headers.
 */

/* Line type IDs */
enum V4L2_MPEG_LINE_TYPES : std::uint8_t {
    V4L2_MPEG_VBI_IVTV_TELETEXT_B     = 1, ///< Teletext (uses lines 6-22 for PAL, 10-21 for NTSC)
    V4L2_MPEG_VBI_IVTV_CAPTION_525    = 4, ///< Closed Captions (line 21 NTSC, line 22 PAL)
    V4L2_MPEG_VBI_IVTV_WSS_625        = 5, ///< Wide Screen Signal (line 20 NTSC, line 23 PAL)
    V4L2_MPEG_VBI_IVTV_VPS            = 7, ///< Video Programming System (PAL) (line 16)
};
// comments for each ID from ivtv_myth.h

#include <QFileInfo>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif // Qt 6

#ifdef _WIN32
#   undef mkdir
#endif

// MythTV headers
#include "libmyth/audio/audiooutput.h"
#include "libmyth/audio/audiooutpututil.h"
#include "libmyth/mythaverror.h"
#include "libmyth/mythavframe.h"
#include "libmythbase/iso639.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/stringutil.h"
#include "libmythui/mythuihelper.h"

#include "mythtvexp.h"

#include "Bluray/mythbdbuffer.h"
#include "DVD/mythdvdbuffer.h"
#include "captions/cc608decoder.h"
#include "captions/cc708decoder.h"
#include "captions/subtitlereader.h"
#include "captions/teletextdecoder.h"
#include "io/mythmediabuffer.h"
#include "mheg/interactivetv.h"
#include "mpeg/atscdescriptors.h"
#include "mpeg/dvbdescriptors.h"
#include "mpeg/mpegtables.h"
#include "bytereader.h"
#include "mythavbufferref.h"
#include "mythavutil.h"
#include "mythframe.h"
#include "mythhdrvideometadata.h"
#include "mythvideoprofile.h"
#include "remoteencoder.h"

using namespace std::string_view_literals;

#define LOC QString("AFD: ")

// Maximum number of sequential invalid data packet errors before we try
// switching to software decoder. Packet errors are often seen when using
// hardware contexts and, for example, seeking. Hence this needs to be high and
// is probably best removed as it is treating the symptoms and not the cause.
// See also comment in MythCodecMap::freeCodecContext re trying to free an
// active hardware context when it is errored.
static constexpr int SEQ_PKT_ERR_MAX { 50 };

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
static constexpr int16_t kMaxVideoQueueSize = 220;
#else
static constexpr ssize_t kMaxVideoQueueSize = 220;
#endif

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
static float get_aspect(AVCParser &p)
{
    static constexpr float kDefaultAspect = 4.0F / 3.0F;
    int asp = p.aspectRatio();
    switch (asp)
    {
        case 0: return kDefaultAspect;
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
            aspect_ratio = kDefaultAspect;
        }
    }
    return aspect_ratio;
}


int  get_avf_buffer(struct AVCodecContext *c, AVFrame *pic, int flags);

// currently unused
//static int determinable_frame_size(struct AVCodecContext *avctx)
//{
//    if (/*avctx->codec_id == AV_CODEC_ID_AAC ||*/
//        avctx->codec_id == AV_CODEC_ID_MP1 ||
//        avctx->codec_id == AV_CODEC_ID_MP2 ||
//        avctx->codec_id == AV_CODEC_ID_MP3/* ||
//        avctx->codec_id == AV_CODEC_ID_CELT*/)
//        return 1;
//    return 0;
//}

#define FAIL(errmsg) do {                       \
    LOG(VB_PLAYBACK, LOG_INFO, LOC + (errmsg)); \
    return false;                                   \
} while (false)

/** @brief Determine if we have enough live TV data to initialize hardware decoders.

This was based on what is now the static int has_codec_parameters() from libavformat/demux.c

*/
static bool StreamHasRequiredParameters(AVCodecContext *Context, AVStream *Stream)
{
    switch (Stream->codecpar->codec_type)
    {
        // We fail on video first as this is generally the most serious error
        // and if we have video, we usually have everything else
        case AVMEDIA_TYPE_VIDEO:
            if (!Context)
                FAIL("No codec for video stream");
            if (!Stream->codecpar->width || !Stream->codecpar->height)
                FAIL("Unspecified video size");
            if (Stream->codecpar->format == AV_PIX_FMT_NONE)
                FAIL("Unspecified video pixel format");
            // The proprietary RealVideo codecs are not used for TV broadcast
            // and codec_info_nb_frames was moved to FFStream as it is an internal, private value.
            //if (Context->codec_id == AV_CODEC_ID_RV30 || Context->codec_id == AV_CODEC_ID_RV40)
            //    if (!Stream->sample_aspect_ratio.num && !Context->sample_aspect_ratio.num && !Stream->codec_info_nb_frames)
            //        FAIL("No frame in rv30/40 and no sar");
            break;
        case AVMEDIA_TYPE_AUDIO:
            if (!Context)
                FAIL("No codec for audio stream");

            // These checks are currently disabled as they continually fail but
            // codec initialisation is fine - which just delays live tv startup.
            // The worst offender appears to be audio description channel...

            //if (!Stream->codecpar->frame_size && determinable_frame_size(avctx))
            //    FAIL("Unspecified audio frame size");
            //if (Stream->codecpar->format == AV_SAMPLE_FMT_NONE)
            //    FAIL("Unspecified audio sample format");
            //if (!Stream->codecpar->sample_rate)
            //    FAIL("Unspecified audio sample rate");
            //if (!Stream->codecpar->channels)
            //    FAIL("Unspecified number of audio channels");
            // if (!Stream->internal->nb_decoded_frames && Context->codec_id == AV_CODEC_ID_DTS)
            //     FAIL("No decodable DTS frames");
            break;

        case AVMEDIA_TYPE_SUBTITLE:
            if (Stream->codecpar->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE && !Stream->codecpar->width)
                FAIL("Unspecified subtitle size");
            break;
        case AVMEDIA_TYPE_DATA:
            if (Stream->codecpar->codec_id == AV_CODEC_ID_NONE)
                return true;
            break;
        default:
            break;
    }

    if (Stream->codecpar->codec_id == AV_CODEC_ID_NONE)
        FAIL("Unknown codec");
    return true;
}

static void myth_av_log(void *ptr, int level, const char* fmt, va_list vl)
{
    if (VERBOSE_LEVEL_NONE())
        return;

    static QString s_fullLine("");
    static QMutex s_stringLock;
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
            verbose_level = LOG_DEBUG;
            break;
        case AV_LOG_TRACE:
            verbose_level = LOG_TRACE;
            break;
        default:
            return;
    }

    if (!VERBOSE_LEVEL_CHECK(verbose_mask, verbose_level))
        return;

    s_stringLock.lock();
    if (s_fullLine.isEmpty() && ptr) {
        AVClass* avc = *(AVClass**)ptr;
        s_fullLine = QString("[%1 @ %2] ")
            .arg(avc->item_name(ptr))
            .arg((quintptr)avc, QT_POINTER_SIZE * 2, 16, QChar('0'));
    }

    s_fullLine += QString::vasprintf(fmt, vl);
    if (s_fullLine.endsWith("\n"))
    {
        LOG(verbose_mask, verbose_level, s_fullLine.trimmed());
        s_fullLine.truncate(0);
    }
    s_stringLock.unlock();
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

/** @brief returns a human readable string for the AVMediaType enum.

av_get_media_type_string() from <libavutil/avutil.h> returns NULL for unknown or
invalid codec_type, so use this instead.
 */
static const char* AVMediaTypeToString(enum AVMediaType codec_type)
{
    switch (codec_type)
    {
        case AVMEDIA_TYPE_UNKNOWN:       return "Unknown";
        case AVMEDIA_TYPE_VIDEO:         return "Video";
        case AVMEDIA_TYPE_AUDIO:         return "Audio";
        case AVMEDIA_TYPE_DATA:          return "Data";
        case AVMEDIA_TYPE_SUBTITLE:      return "Subtitle";
        case AVMEDIA_TYPE_ATTACHMENT:    return "Attachment";
        default:                         return "Invalid Codec Type";
    }
}

AvFormatDecoder::AvFormatDecoder(MythPlayer *parent,
                                 const ProgramInfo &pginfo,
                                 PlayerFlags flags)
    : DecoderBase(parent, pginfo),
      m_isDbIgnored(gCoreContext->IsDatabaseIgnored()),
      m_avcParser(new AVCParser()),
      m_playerFlags(flags),
      // Closed Caption & Teletext decoders
      m_ccd608(new CC608Decoder(parent->GetCC608Reader())),
      m_ccd708(new CC708Decoder(parent->GetCC708Reader())),
      m_ttd(new TeletextDecoder(parent->GetTeletextReader())),
      m_itv(parent->GetInteractiveTV()),
      m_audioSamples((uint8_t *)av_mallocz(AudioOutput::kMaxSizeBuffer))
{
    // this will be deleted and recreated once decoder is set up
    m_mythCodecCtx = new MythCodecContext(this, kCodec_NONE);

    m_ccd608->SetIgnoreTimecode(true);

    av_log_set_callback(myth_av_log);

    m_audioIn.m_sampleSize = -32;// force SetupAudioStream to run once

    AvFormatDecoder::SetIdrOnlyKeyframes(true);
    m_audioReadAhead = gCoreContext->GetDurSetting<std::chrono::milliseconds>("AudioReadAhead", 100ms);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PlayerFlags: 0x%1, AudioReadAhead: %2 msec")
        .arg(m_playerFlags, 0, 16).arg(m_audioReadAhead.count()));
}

AvFormatDecoder::~AvFormatDecoder()
{
    while (!m_storedPackets.isEmpty())
    {
        AVPacket *pkt = m_storedPackets.takeFirst();
        av_packet_free(&pkt);
    }

    CloseContext();
    delete m_ccd608;
    delete m_ccd708;
    delete m_ttd;
    delete m_avcParser;
    delete m_mythCodecCtx;

    sws_freeContext(m_swsCtx);

    av_freep(reinterpret_cast<void*>(&m_audioSamples));

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

MythCodecMap* AvFormatDecoder::CodecMap(void)
{
    return &m_codecMap;
}

void AvFormatDecoder::CloseCodecs()
{
    if (m_ic)
    {
        m_avCodecLock.lock();
        for (uint i = 0; i < m_ic->nb_streams; i++)
        {
            AVStream *st = m_ic->streams[i];
            m_codecMap.FreeCodecContext(st);
        }
        m_avCodecLock.unlock();
    }
}

void AvFormatDecoder::CloseContext()
{
    if (m_ic)
    {
        CloseCodecs();

        delete m_avfRingBuffer;
        m_avfRingBuffer = nullptr;
        m_ic->pb = nullptr;
        avformat_close_input(&m_ic);
        m_ic = nullptr;
    }
    m_avcParser->Reset();
}

static int64_t lsb3full(int64_t lsb, int64_t base_ts, int lsb_bits)
{
    int64_t mask = (lsb_bits < 64) ? (1LL<<lsb_bits)-1 : -1LL;
    return  ((lsb - base_ts)&mask);
}

std::chrono::milliseconds AvFormatDecoder::NormalizeVideoTimecode(std::chrono::milliseconds timecode)
{
    int64_t start_pts = 0;

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
        return 0ms;

    if (m_ic->start_time != AV_NOPTS_VALUE)
    {
        start_pts = av_rescale(m_ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);
    }

    int64_t pts = av_rescale(timecode.count() / 1000.0,
                     st->time_base.den,
                     st->time_base.num);

    // adjust for start time and wrap
    pts = lsb3full(pts, start_pts, st->pts_wrap_bits);

    return millisecondsFromFloat(av_q2d(st->time_base) * pts * 1000);
}

std::chrono::milliseconds AvFormatDecoder::NormalizeVideoTimecode(AVStream *st,
                                                std::chrono::milliseconds timecode)
{
    int64_t start_pts = 0;

    if (m_ic->start_time != AV_NOPTS_VALUE)
    {
        start_pts = av_rescale(m_ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);
    }

    int64_t pts = av_rescale(timecode.count() / 1000.0,
                     st->time_base.den,
                     st->time_base.num);

    // adjust for start time and wrap
    pts = lsb3full(pts, start_pts, st->pts_wrap_bits);

    return millisecondsFromFloat(av_q2d(st->time_base) * pts * 1000);
}

int AvFormatDecoder::GetNumChapters()
{
    if (m_ic && m_ic->nb_chapters > 1)
        return m_ic->nb_chapters;
    return 0;
}

void AvFormatDecoder::GetChapterTimes(QList<std::chrono::seconds> &times)
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
        times.push_back(std::chrono::seconds((long long)total_secs));
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
        auto framenum = (long long)(total_secs * m_fps);
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
    auto framenum = (long long)(total_secs * m_fps);
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

    // avformat-based seeking
    return do_av_seek(desiredFrame, discardFrames, AVSEEK_FLAG_BACKWARD);
}

bool AvFormatDecoder::DoFastForward(long long desiredFrame, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DoFastForward(%1 (%2), %3 discard frames)")
            .arg(desiredFrame).arg(m_framesPlayed)
            .arg((discardFrames) ? "do" : "don't"));

    if (m_recordingHasPositionMap || m_livetv)
        return DecoderBase::DoFastForward(desiredFrame, discardFrames);

    int seekDelta = desiredFrame - m_framesPlayed;

    // avoid using av_frame_seek if we are seeking frame-by-frame when paused
    if (seekDelta >= 0 && seekDelta < 2 && m_parent->GetPlaySpeed() == 0.0F)
    {
        SeekReset(m_framesPlayed, seekDelta, false, true);
        m_parent->SetFramesPlayed(m_framesPlayed + 1);
        return true;
    }
    return do_av_seek(desiredFrame, discardFrames, 0);
}

bool AvFormatDecoder::do_av_seek(long long desiredFrame, bool discardFrames, int flags)
{
    long long ts = 0;
    if (m_ic->start_time != AV_NOPTS_VALUE)
        ts = m_ic->start_time;

    // convert framenumber to normalized timestamp
    long double seekts = desiredFrame * AV_TIME_BASE / m_fps;
    ts += (long long)seekts;

    // XXX figure out how to do snapping in this case
    bool exactseeks = DecoderBase::GetSeekSnap() == 0U;

    if (exactseeks)
    {
        flags |= AVSEEK_FLAG_BACKWARD;
    }

    int ret = av_seek_frame(m_ic, -1, ts, flags);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_seek_frame(m_ic, -1, %1, 0b%2) error: %3").arg(
                QString::number(ts),
                QString::number(flags, 2),
                QString::fromStdString(av_make_error_stdstring(ret))
                )
            );
        return false;
    }
    if (auto* reader = m_parent->GetSubReader(); reader)
        reader->SeekFrame(ts, flags);

    int normalframes = 0;

    {
        m_framesPlayed = desiredFrame;
        m_fpsSkip = 0;
        m_framesRead = desiredFrame;
        normalframes = 0;
    }

    SeekReset(m_lastKey, normalframes, true, discardFrames);

    if (discardFrames)
        m_parent->SetFramesPlayed(m_framesPlayed + 1);

    return true;
}

void AvFormatDecoder::SeekReset(long long newKey, uint skipFrames,
                                bool doflush, bool discardFrames)
{
    if (!m_ringBuffer)
        return; // nothing to reset...

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("SeekReset(%1, %2, %3 flush, %4 discard)")
            .arg(newKey).arg(skipFrames)
            .arg((doflush) ? "do" : "don't",
                 (discardFrames) ? "do" : "don't"));

    DecoderBase::SeekReset(newKey, skipFrames, doflush, discardFrames);

    QMutexLocker locker(&m_avCodecLock);

    // Discard all the queued up decoded frames
    if (discardFrames)
    {
        bool releaseall = m_mythCodecCtx ? (m_mythCodecCtx->DecoderWillResetOnFlush() ||
                                            m_mythCodecCtx->DecoderNeedsReset(nullptr)) : false;
        m_parent->DiscardVideoFrames(doflush, doflush && releaseall);
    }

    if (doflush)
    {
        m_lastAPts = 0ms;
        m_lastVPts = 0ms;
        m_lastCcPtsu = 0us;

        avformat_flush(m_ic);

        // Only reset the internal state if we're using our seeking,
        // not when using libavformat's seeking
        if (m_recordingHasPositionMap || m_livetv)
        {
            m_ic->pb->pos = m_ringBuffer->GetReadPosition();
            m_ic->pb->buf_ptr = m_ic->pb->buffer;
            m_ic->pb->buf_end = m_ic->pb->buffer;
            m_ic->pb->eof_reached = 0;
        }

        // Flush the avcodec buffers
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "SeekReset() flushing");
        for (uint i = 0; i < m_ic->nb_streams; i++)
        {
            AVCodecContext *codecContext = m_codecMap.FindCodecContext(m_ic->streams[i]);
            // note that contexts that have not been opened have
            // codecContext->internal = nullptr and cause a segfault in
            // avcodec_flush_buffers
            if (codecContext && codecContext->internal)
                avcodec_flush_buffers(codecContext);
        }

        // Free up the stored up packets
        while (!m_storedPackets.isEmpty())
        {
            AVPacket *pkt = m_storedPackets.takeFirst();
            av_packet_free(&pkt);
        }

        m_prevGopPos = 0;
        m_gopSet = false;
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
    static constexpr std::chrono::milliseconds maxSeekTimeMs { 200ms };
    int profileFrames = 0;
    MythTimer begin(MythTimer::kStartRunning);
    for (; (skipFrames > 0 && !m_atEof &&
            (exactSeeks || begin.elapsed() < maxSeekTimeMs));
         --skipFrames, ++profileFrames)
    {
        // TODO this won't work well in conjunction with the MythTimer
        // above...
        QElapsedTimer getframetimer;
        getframetimer.start();
        bool retry = true;
        while (retry && !getframetimer.hasExpired(100))
        {
            retry = false;
            GetFrame(kDecodeVideo, retry);
            if (retry)
                std::this_thread::sleep_for(1ms);
        }

        if (m_decodedVideoFrame)
        {
            m_parent->DiscardVideoFrame(m_decodedVideoFrame);
            m_decodedVideoFrame = nullptr;
        }
        if (!exactSeeks && profileFrames >= 5 && profileFrames < 10)
        {
            const int giveUpPredictionMs = 400;
            int remainingTimeMs =
                skipFrames * (float)begin.elapsed().count() / profileFrames;
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
        m_firstVPts = 0ms;
        m_firstVPtsInuse = true;
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
        m_seenGop = false;
        m_seqCount = 0;
    }
}

bool AvFormatDecoder::CanHandle(TestBufferVec & testbuf, const QString &filename)
{
    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));

    QByteArray fname = filename.toLatin1();
    probe.filename = fname.constData();
    probe.buf = (unsigned char *)testbuf.data();
    probe.buf_size = testbuf.size();

    int score = AVPROBE_SCORE_MAX/4;

    if (testbuf.size() + AVPROBE_PADDING_SIZE > kDecoderProbeBufferSize)
    {
        probe.buf_size = kDecoderProbeBufferSize - AVPROBE_PADDING_SIZE;
        score = 0;
    }
    else if (testbuf.size()*2 >= kDecoderProbeBufferSize)
    {
        score--;
    }

    memset(probe.buf + probe.buf_size, 0, AVPROBE_PADDING_SIZE);

    return av_probe_input_format2(&probe, static_cast<int>(true), &score) != nullptr;
}

void AvFormatDecoder::streams_changed(void *data, int avprogram_id)
{
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(data);

    int cnt = decoder->m_ic->nb_streams;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("streams_changed 0x%1 -- program_number %2 stream count %3")
            .arg((uint64_t)data,0,16).arg(QString::number(avprogram_id), QString::number(cnt)));

    auto* program = decoder->get_current_AVProgram();
    if (program != nullptr && program->id != avprogram_id)
    {
        return;
    }
    decoder->m_streamsChanged = true;
}

extern "C"
{
    static void HandleStreamChange(void *data, int avprogram_id)
    {
        AvFormatDecoder::streams_changed(data, avprogram_id);
    }
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
int AvFormatDecoder::OpenFile(MythMediaBuffer *Buffer, bool novideo,
                              TestBufferVec & testbuf)
{
    CloseContext();

    m_ringBuffer = Buffer;

    // Process frames immediately unless we're decoding
    // a DVD, in which case don't so that we don't show
    // anything whilst probing the data streams.
    m_processFrames = !m_ringBuffer->IsDVD();

    const AVInputFormat *fmt = nullptr;
    QString        fnames   = m_ringBuffer->GetFilename();
    QByteArray     fnamea   = fnames.toLatin1();
    const char    *filename = fnamea.constData();

    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));
    probe.filename = filename;
    probe.buf = reinterpret_cast<unsigned char *>(testbuf.data());
    if (testbuf.size() + AVPROBE_PADDING_SIZE <= kDecoderProbeBufferSize)
        probe.buf_size = testbuf.size();
    else
        probe.buf_size = kDecoderProbeBufferSize - AVPROBE_PADDING_SIZE;
    memset(probe.buf + probe.buf_size, 0, AVPROBE_PADDING_SIZE);

    fmt = av_probe_input_format(&probe, static_cast<int>(true));
    if (!fmt)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Probe failed for '%1'").arg(filename));
        return -1;
    }

    if (strcmp(fmt->name, "mpegts") == 0 &&
        gCoreContext->GetBoolSetting("FFMPEGTS", false))
    {
        const AVInputFormat *fmt2 = av_find_input_format("mpegts-ffmpeg");
        if (fmt2)
        {
            fmt = fmt2;
            LOG(VB_GENERAL, LOG_INFO, LOC + "Using FFmpeg MPEG-TS demuxer (forced)");
        }
    }

    int err = 0;
    bool scancomplete = false;
    int  remainingscans  = 5;

    while (!scancomplete && remainingscans--)
    {
        bool found = false;

        // With live tv, the ringbufer may contain insufficient data for complete
        // initialisation so we try a few times with a slight pause each time to
        // allow extra data to become available. In the worst case scenarios, the
        // stream may not have a keyframe for 4-5 seconds.
        // As a last resort, we will try and fallback to the original FFmpeg MPEG-TS
        // demuxer if it is not already used.
        // For regular videos, this shouldn't be an issue as the complete file
        // should be available - though we try a little harder for streamed formats
        int retries = m_livetv || m_ringBuffer->IsStreamed() ? 50 : 10;

        while (!found && --retries)
        {
            m_ic = avformat_alloc_context();
            if (!m_ic)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Could not allocate format context.");
                return -1;
            }

            delete m_avfRingBuffer;
            m_avfRingBuffer = new MythAVFormatBuffer(m_ringBuffer, false, m_livetv);
            m_ic->pb = m_avfRingBuffer->getAVIOContext();
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Buffer size: %1 Streamed %2 Seekable %3 Available %4")
                .arg(m_ringBuffer->BestBufferSize())
                .arg(m_ringBuffer->IsStreamed())
                .arg(m_ic->pb->seekable)
                .arg(m_ringBuffer->GetReadBufAvail()));
            m_avfRingBuffer->SetInInit(false);

            err = avformat_open_input(&m_ic, filename, fmt, nullptr);
            if (err < 0)
            {
                std::string error;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Failed to open input ('%1')")
                    .arg(av_make_error_stdstring(error, err)));

                // note - m_ic (AVFormatContext) is freed on failure
                if (retries > 2)
                {
                    // wait a little to buffer more data
                    // 50*0.1 = 5 seconds max
                    QThread::usleep(100000);
                    // resets the read position
                    m_avfRingBuffer->SetInInit(false);
                    continue;
                }

                if (strcmp(fmt->name, "mpegts") == 0)
                {
                    fmt = av_find_input_format("mpegts-ffmpeg");
                    if (fmt)
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC + "Attempting to use original FFmpeg MPEG-TS demuxer.");
                        // resets the read position
                        m_avfRingBuffer->SetInInit(false);
                        continue;
                    }
                    break;
                }
            }
            found = true;
        }

        if (err < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error opening input. Aborting");
            m_ic = nullptr;
            return -1;
        }

        // With certain streams, we don't get a complete stream analysis and the video
        // codec/frame format is not fully detected. This can have various consequences - from
        // failed playback to not enabling hardware decoding (as the frame formt is not valid).
        // Bump the duration (FFmpeg defaults to 5 seconds) to 60 seconds. This should
        // not impact performance as in the vast majority of cases the scan is completed
        // within a second or two (seconds in this case referring to stream duration - not the time
        // it takes to complete the scan).
        m_ic->max_analyze_duration = 60LL * AV_TIME_BASE;

        m_avfRingBuffer->SetInInit(m_livetv);
        m_avCodecLock.lock();
        err = avformat_find_stream_info(m_ic, nullptr);
        m_avCodecLock.unlock();
        if (err < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Could not find codec parameters for '%1'").arg(filename));
            CloseContext();
            return -1;
        }
        m_avfRingBuffer->SetInInit(false);

        // final sanity check that scanned streams are valid for live tv
        scancomplete = true;
        for (uint i = 0; m_livetv && (i < m_ic->nb_streams); i++)
        {
            if (!StreamHasRequiredParameters(m_codecMap.GetCodecContext(m_ic->streams[i]), m_ic->streams[i]))
            {
                scancomplete = false;
                if (remainingscans)
                {
                    CloseContext();
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Stream scan incomplete - retrying");
                    QThread::usleep(250000); // wait 250ms
                }
                break;
            }
        }
    }

    if (!scancomplete)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Scan incomplete - playback may not work");

    m_ic->streams_changed = HandleStreamChange;
    m_ic->stream_change_data = this;

    if (!m_livetv && !m_ringBuffer->IsDisc())
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
    err = ScanStreams(novideo);
    if (-1 == err)
    {
        CloseContext();
        return err;
    }

#if CONFIG_MHEG
    {
        int initialAudio = -1;
        int initialVideo = -1;
        if (m_itv == nullptr)
            m_itv = m_parent->GetInteractiveTV();
        if (m_itv != nullptr)
            m_itv->GetInitialStreams(initialAudio, initialVideo);
        if (initialAudio >= 0)
            SetAudioByComponentTag(initialAudio);
        if (initialVideo >= 0)
            SetVideoByComponentTag(initialVideo);
    }
#endif // CONFIG_MHEG

    // Try to get a position map from the recorder if we don't have one yet.
    if (!m_recordingHasPositionMap && !m_isDbIgnored)
    {
        if ((m_playbackInfo) || m_livetv || m_watchingRecording)
        {
            m_recordingHasPositionMap |= SyncPositionMap();
            if (m_recordingHasPositionMap && !m_livetv && !m_watchingRecording)
            {
                m_hasFullPositionMap = true;
                m_gopSet = true;
            }
        }
    }

    // If watching pre-recorded television or video use the marked duration
    // from the db if it exists, else ffmpeg duration
    std::chrono::seconds dur = 0s;

    if (m_playbackInfo)
    {
        dur = duration_cast<std::chrono::seconds>(m_playbackInfo->QueryTotalDuration());
    }

    if (dur == 0s)
    {
        dur = duration_cast<std::chrono::seconds>(av_duration(m_ic->duration));
    }

    if (dur > 0s && !m_livetv && !m_watchingRecording)
    {
        m_parent->SetDuration(dur);
    }

    // If we don't have a position map, set up ffmpeg for seeking
    if (!m_recordingHasPositionMap && !m_livetv)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Recording has no position -- using libavformat seeking.");

        if (dur > 0s)
        {
            m_parent->SetFileLength(dur, (int)(dur.count() * m_fps));
        }
        else
        {
            // the pvr-250 seems to over report the bitrate by * 2
            float bytespersec = (float)m_bitrate / 8 / 2;
            float secs = m_ringBuffer->GetRealFileSize() * 1.0F / bytespersec;
            m_parent->SetFileLength(secondsFromFloat(secs),
                                    (int)(secs * static_cast<float>(m_fps)));
        }

        // we will not see a position map from db or remote encoder,
        // set the gop interval to 15 frames.  if we guess wrong, the
        // auto detection will change it.
        m_keyframeDist = 15;
        m_positionMapType = MARK_GOP_BYFRAME;

        if (strcmp(fmt->name, "avi") == 0)
        {
            // avi keyframes are too irregular
            m_keyframeDist = 1;
        }

        m_dontSyncPositionMap = true;
    }

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
        auto total_secs = static_cast<long double>(start) * static_cast<long double>(num) /
                          static_cast<long double>(den);
        auto msec = millisecondsFromFloat(total_secs * 1000);
        auto framenum = static_cast<long long>(total_secs * static_cast<long double>(m_fps));
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Chapter %1 found @ [%2]->%3")
                .arg(StringUtil::intToPaddedString(i + 1),
                     MythDate::formatTime(msec, "HH:mm:ss.zzz"),
                     QString::number(framenum)));
    }

    if (m_ringBuffer->IsDVD())
    {
        // Reset DVD playback and clear any of
        // our buffers so that none of the data
        // parsed so far to determine decoders
        // gets shown.
        if (!m_ringBuffer->StartFromBeginning())
            return -1;
        m_ringBuffer->IgnoreWaitStates(false);

        Reset(true, true, true);

        // Now we're ready to process and show frames
        m_processFrames = true;
    }


    // Return true if recording has position map
    return static_cast<int>(m_recordingHasPositionMap);
}

float AvFormatDecoder::GetVideoFrameRate(AVStream *Stream, AVCodecContext *Context, bool Sanitise)
{
    // MKV default_duration
    double avg_fps       = (Stream->avg_frame_rate.den == 0) ? 0.0 : av_q2d(Stream->avg_frame_rate);
    double codec_fps     = av_q2d(Context->framerate); // {0, 1} when unknown
    double container_fps = (Stream->time_base.num == 0) ? 0.0 : av_q2d(av_inv_q(Stream->time_base));
    // least common multiple of all framerates in a stream; this is a guess
    double estimated_fps = (Stream->r_frame_rate.den == 0) ? 0.0 : av_q2d(Stream->r_frame_rate);

    // build a list of possible rates, best first
    std::vector<double> rates;
    rates.reserve(7);

    // matroska demuxer sets the default_duration to avg_frame_rate
    // mov,mp4,m4a,3gp,3g2,mj2 demuxer sets avg_frame_rate
    if (QString(m_ic->iformat->name).contains("matroska") ||
        QString(m_ic->iformat->name).contains("mov"))
    {
        rates.emplace_back(avg_fps);
    }

    // avi uses container fps for timestamps
    if (QString(m_ic->iformat->name).contains("avi"))
    {
        rates.emplace_back(container_fps);
    }

    rates.emplace_back(codec_fps);
    rates.emplace_back(container_fps);
    rates.emplace_back(avg_fps);
    // certain H.264 interlaced streams are detected at 2x using estimated (i.e. wrong)
    rates.emplace_back(estimated_fps);
    // last resort, default to NTSC
    rates.emplace_back(30000.0 / 1001.0);

    auto invalid_fps = [](double rate) { return rate < 3.0 || rate > 121.0; };
    rates.erase(std::remove_if(rates.begin(), rates.end(), invalid_fps), rates.end());

    auto FuzzyEquals = [](double First, double Second) { return std::abs(First - Second) < 0.03; };

    // debug
    if (!FuzzyEquals(rates.front(), m_fps))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Selected FPS: %1 (Avg:%2 Mult:%3 Codec:%4 Container:%5 Estimated:%6)")
                .arg(static_cast<double>(rates.front())).arg(avg_fps)
                .arg(m_fpsMultiplier).arg(codec_fps).arg(container_fps).arg(estimated_fps));
    }

    auto IsStandard = [&FuzzyEquals](double Rate)
    {
        // List of known, standards based frame rates
        static const std::set<double> k_standard_rates =
        {
            24000.0 / 1001.0,
            23.976,
            24.0,
            25.0,
            30000.0 / 1001.0,
            29.97,
            30.0,
            50.0,
            60000.0 / 1001.0,
            59.94,
            60.0,
            100.0,
            120000.0 / 1001.0,
            119.88,
            120.0
        };

        if (Rate > 23.0 && Rate < 121.0)
        {
            for (auto standard_rate : k_standard_rates)
                if (FuzzyEquals(Rate, standard_rate))
                    return true;
        }
        return false;
        // TODO do not convert AVRational to double
        //return k_standard_rates.find(rate) != k_standard_rates.end();
    };

    // If the first choice rate is unusual, see if there is something more 'usual'
    double detected = rates.front();
    if (Sanitise && !IsStandard(detected))
    {
        for (auto rate : rates)
        {
            if (IsStandard(rate))
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 is non-standard - using %2 instead.")
                    .arg(rates.front()).arg(rate));

                // The most common problem here is mpegts files where the average
                // rate is slightly out and the estimated rate is the fallback.
                // As noted above, however, the estimated rate is sometimes twice
                // the actual for interlaced content. Try and detect and fix this
                // so that we don't throw out deinterlacing and video mode switching.
                // Assume anything under 30 may be interlaced - with +-10% error.
                if (rate > 33.0 && detected < 33.0)
                {
                    double half = rate / 2.0;
                    if (std::abs(half - detected) < (half * 0.1))
                    {
                        LOG(VB_GENERAL, LOG_INFO, LOC +
                            QString("Assuming %1 is a better choice than %2")
                            .arg(half).arg(rate));
                        return static_cast<float>(half);
                    }
                }
                return static_cast<float>(rate);
            }
        }
    }

    return static_cast<float>(detected);
}

int AvFormatDecoder::GetMaxReferenceFrames(AVCodecContext *Context)
{
    switch (Context->codec_id)
    {
        case AV_CODEC_ID_H264:
        {
            int result = 16;
            if (Context->extradata && (Context->extradata_size >= 7))
            {
                uint8_t offset = 0;
                if (Context->extradata[0] == 1)
                    offset = 9; // avCC
                else if (AV_RB24(Context->extradata) == 0x01) // Annex B - 3 byte startcode 0x000001
                    offset = 4;
                else if (AV_RB32(Context->extradata) == 0x01) // Annex B - 4 byte startcode 0x00000001
                    offset= 5;

                if (offset)
                {
                    AVCParser parser;
                    bool dummy = false;
                    parser.parse_SPS(Context->extradata + offset,
                                     static_cast<uint>(Context->extradata_size - offset), dummy, result);
                }
            }
            return result;
        }
        case AV_CODEC_ID_H265: return 16;
        case AV_CODEC_ID_VP9:  return 8;
        case AV_CODEC_ID_VP8:  return 3;
        default:               return 2;
    }
}

void AvFormatDecoder::InitVideoCodec(AVStream *stream, AVCodecContext *codecContext,
                                     bool selectedStream)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InitVideoCodec ID:%1 Type:%2 Size:%3x%4")
            .arg(avcodec_get_name(codecContext->codec_id),
                 AVMediaTypeToString(codecContext->codec_type))
            .arg(codecContext->width).arg(codecContext->height));

    if (m_ringBuffer && m_ringBuffer->IsDVD())
        m_directRendering = false;

    codecContext->opaque = static_cast<void*>(this);
    codecContext->get_buffer2 = get_avf_buffer;
    codecContext->slice_flags = 0;

    codecContext->err_recognition = AV_EF_COMPLIANT;
    codecContext->workaround_bugs = FF_BUG_AUTODETECT;
    codecContext->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    codecContext->idct_algo = FF_IDCT_AUTO;
    codecContext->debug = 0;
    // codecContext->error_rate = 0;

    const AVCodec *codec1 = codecContext->codec;

    if (selectedStream)
        m_directRendering = true;

    // retrieve rotation information
    const AVPacketSideData *sd = av_packet_side_data_get(stream->codecpar->coded_side_data,
        stream->codecpar->nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX);
    if (sd)
        m_videoRotation = static_cast<int>(-av_display_rotation_get(reinterpret_cast<int32_t*>(sd->data)));
    else
        m_videoRotation = 0;

    // retrieve 3D type
    sd = av_packet_side_data_get(stream->codecpar->coded_side_data,
        stream->codecpar->nb_coded_side_data, AV_PKT_DATA_STEREO3D);
    if (sd)
    {
        auto * avstereo = reinterpret_cast<AVStereo3D*>(sd->data);
        m_stereo3D = avstereo->type;
    }

    delete m_mythCodecCtx;
    m_mythCodecCtx = MythCodecContext::CreateContext(this, m_videoCodecId);
    m_mythCodecCtx->InitVideoCodec(codecContext, selectedStream, m_directRendering);
    if (m_mythCodecCtx->HwDecoderInit(codecContext) < 0)
    {
        // force it to switch to software decoding
        m_averrorCount = SEQ_PKT_ERR_MAX + 1;
        m_streamsChanged = true;
    }
    else
    {
        // Note: This is never going to work as expected in all circumstances.
        // It will not account for changes in the stream and/or display. For
        // MediaCodec, Android will do its own thing (and judging by the Android logs,
        // shouldn't double rate the deinterlacing if the display cannot support it).
        // NVDEC will probably move to the FFmpeg YADIF CUDA deinterlacer - which
        // will avoid the issue (video player deinterlacing) and maybe disable
        // decoder VAAPI deinterlacing. If we get it wrong and the display cannot
        // keep up, the player should just drop frames.

        // FIXME - need a better way to handle this
        bool doublerate = true;//m_parent->CanSupportDoubleRate();
        m_mythCodecCtx->SetDeinterlacing(codecContext, &m_videoDisplayProfile, doublerate);
    }

    if (codec1 && ((AV_CODEC_ID_MPEG2VIDEO == codec1->id) ||
                   (AV_CODEC_ID_MPEG1VIDEO == codec1->id)))
    {
        if (FlagIsSet(kDecodeFewBlocks))
        {
            int total_blocks = (codecContext->height + 15) / 16;
            codecContext->skip_top    = (total_blocks + 3) / 4;
            codecContext->skip_bottom = (total_blocks + 3) / 4;
        }

        if (FlagIsSet(kDecodeLowRes))
            codecContext->lowres = 2; // 1 = 1/2 size, 2 = 1/4 size
    }
    else if (codec1 && (AV_CODEC_ID_H264 == codec1->id) && FlagIsSet(kDecodeNoLoopFilter))
    {
        codecContext->flags &= ~AV_CODEC_FLAG_LOOP_FILTER;
        codecContext->skip_loop_filter = AVDISCARD_ALL;
    }

    if (FlagIsSet(kDecodeNoDecode))
        codecContext->skip_idct = AVDISCARD_ALL;

    if (selectedStream)
    {
        // m_fps is now set 'correctly' in ScanStreams so this additional call
        // to GetVideoFrameRate may now be redundant
        m_fps = GetVideoFrameRate(stream, codecContext, true);
        QSize dim    = get_video_dim(*codecContext);
        int   width  = m_currentWidth  = dim.width();
        int   height = m_currentHeight = dim.height();
        m_currentAspect = get_aspect(*codecContext);

        if (!width || !height)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "InitVideoCodec invalid dimensions, resetting decoder.");
            width  = 640;
            height = 480;
            m_fps    = 29.97F;
            m_currentAspect = 4.0F / 3.0F;
        }

        m_parent->SetKeyframeDistance(m_keyframeDist);
        const AVCodec *codec2 = codecContext->codec;
        QString codecName;
        if (codec2)
            codecName = codec2->name;
        m_parent->SetVideoParams(width, height, m_fps,
                                 m_currentAspect, false, GetMaxReferenceFrames(codecContext),
                                 kScan_Detect, codecName);
        if (LCD *lcd = LCD::Get())
        {
            LCDVideoFormatSet video_format = VIDEO_MPG;

            switch (codecContext->codec_id)
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

static bool cc608_good_parity(uint16_t data)
{
    static constexpr std::array<uint8_t, 256> odd_parity_LUT
    {
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
         0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    };
    bool ret = (odd_parity_LUT[data & 0xff]          == 1) &&
               (odd_parity_LUT[(data & 0xff00) >> 8] == 1);
    if (!ret)
    {
        LOG(VB_VBI, LOG_ERR, LOC +
            QString("VBI: Bad parity in EIA-608 data (%1)") .arg(data,0,16));
    }
    return ret;
}

static AVBufferRef* get_pmt_section_from_AVProgram(const AVProgram *program)
{
    if (program == nullptr)
    {
        return nullptr;
    }
    return program->pmt_section;
}

static AVBufferRef* get_pmt_section_for_AVStream_index(AVFormatContext *context, int stream_index)
{
    AVProgram* program = av_find_program_from_stream(context, nullptr, stream_index);
    return get_pmt_section_from_AVProgram(program);
}

void AvFormatDecoder::ScanATSCCaptionStreams(int av_index)
{
    QMutexLocker locker(&m_trackLock);

    m_ccX08InPmt.fill(false);
    m_pmtTracks.clear();
    m_pmtTrackTypes.clear();

    // Figure out languages of ATSC captions
    MythAVBufferRef pmt_buffer {get_pmt_section_for_AVStream_index(m_ic, av_index)};
    if (!pmt_buffer.has_buffer())
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "ScanATSCCaptionStreams() called with no PMT");
        return;
    }
    const ProgramMapTable pmt(PSIPTable(pmt_buffer.data()));

    bool video_found = false;
    uint i = 0;
    for (i = 0; i < pmt.StreamCount(); i++)
    {
        // MythTV remaps OpenCable Video to normal video during recording
        // so "dvb" is the safest choice for system info type, since this
        // will ignore other uses of the same stream id in DVB countries.
        if (pmt.IsVideo(i, "dvb"))
        {
            video_found = true;
            break;
        }
    }
    if (!video_found)
        return;

    desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
        pmt.StreamInfo(i), pmt.StreamInfoLength(i),
        DescriptorID::caption_service);

    const desc_list_t desc_list2 = MPEGDescriptor::ParseOnlyInclude(
        pmt.ProgramInfo(), pmt.ProgramInfoLength(),
        DescriptorID::caption_service);

    desc_list.insert(desc_list.end(), desc_list2.begin(), desc_list2.end());

    for (auto & desc : desc_list)
    {
        const CaptionServiceDescriptor csd(desc);
        if (!csd.IsValid())
            continue;

        LOG(VB_VBI, LOG_DEBUG, LOC + csd.toString());

        for (uint k = 0; k < csd.ServicesCount(); k++)
        {
            int lang = csd.CanonicalLanguageKey(k);
            int type = csd.Type(k) ? 1 : 0;
            if (type)
            {
                StreamInfo si {av_index, csd.CaptionServiceNumber(k), lang};
                uint key = csd.CaptionServiceNumber(k) + 4;
                m_ccX08InPmt[key] = true;
                m_pmtTracks.push_back(si);
                m_pmtTrackTypes.push_back(kTrackTypeCC708);
            }
            else
            {
                int line21 = csd.Line21Field(k) ? 3 : 1;
                StreamInfo si {av_index, line21, lang};
                m_ccX08InPmt[line21-1] = true;
                m_pmtTracks.push_back(si);
                m_pmtTrackTypes.push_back(kTrackTypeCC608);
            }
        }
    }
}

void AvFormatDecoder::UpdateATSCCaptionTracks(void)
{
    QMutexLocker locker(&m_trackLock);

    m_tracks[kTrackTypeCC608].clear();
    m_tracks[kTrackTypeCC708].clear();
    m_ccX08InTracks.fill(false);

    uint pidx = 0;
    uint sidx = 0;
    std::array<std::map<int,uint>,2> lang_cc_cnt;
    while (true)
    {
        bool pofr = pidx >= (uint)m_pmtTracks.size();
        bool sofr = sidx >= (uint)m_streamTracks.size();
        if (pofr && sofr)
            break;

        // choose lowest available next..
        // stream_id's of 608 and 708 streams alias, but this
        // is ok as we just want each list to be ordered.
        StreamInfo const *si = nullptr;
        int type = 0; // 0 if 608, 1 if 708
        bool isp = true; // if true use m_pmtTracks next, else stream_tracks

        if (pofr && !sofr)
            isp = false; // NOLINT(bugprone-branch-clone)
        else if (!pofr && sofr)
            isp = true;
        else if (m_streamTracks[sidx] < m_pmtTracks[pidx])
            isp = false;

        if (isp)
        {
            si = &m_pmtTracks[pidx];
            type = kTrackTypeCC708 == m_pmtTrackTypes[pidx] ? 1 : 0;
            pidx++;
        }
        else
        {
            si = &m_streamTracks[sidx];
            type = kTrackTypeCC708 == m_streamTrackTypes[sidx] ? 1 : 0;
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
            m_ccX08InTracks[key] = true;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("%1 caption service #%2 is in the %3 language.")
                .arg((type) ? "EIA-708" : "EIA-608")
                .arg(nsi.m_stream_id)
                .arg(iso639_key_toName(nsi.m_language)));
    }
    AutoSelectTrack(kTrackTypeCC608);
    AutoSelectTrack(kTrackTypeCC708);
}

void AvFormatDecoder::ScanTeletextCaptions(int av_index)
{
    QMutexLocker locker(&m_trackLock);

    // ScanStreams() calls m_tracks[kTrackTypeTeletextCaptions].clear()
    if (!m_tracks[kTrackTypeTeletextCaptions].empty())
        return;

    AVStream* st = m_ic->streams[av_index];
    const AVDictionaryEntry* language_dictionary_entry =
        av_dict_get(st->metadata, "language", nullptr, 0);

    if (language_dictionary_entry == nullptr ||
        language_dictionary_entry->value == nullptr ||
        st->codecpar->extradata == nullptr
       )
    {
        return;
    }

    std::vector<std::string_view> languages {StringUtil::split_sv(language_dictionary_entry->value, ","sv)};

    if (st->codecpar->extradata_size != static_cast<int>(languages.size() * 2))
    {
        return;
    }
    for (size_t i = 0; i < languages.size(); i++)
    {
        if (languages[i].size() != 3)
        {
            continue;
        }
        //NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
        int language = iso639_str3_to_key(languages[i].data());
        uint8_t teletext_type = st->codecpar->extradata[i * 2] >> 3;
        uint8_t teletext_magazine_number = st->codecpar->extradata[i * 2] & 0x7;
        if (teletext_magazine_number == 0)
            teletext_magazine_number = 8;
        uint8_t teletext_page_number = st->codecpar->extradata[(i * 2) + 1];
        if (teletext_type == 2 || teletext_type == 1)
        {
            TrackType track = (teletext_type == 2) ?
                                kTrackTypeTeletextCaptions :
                                kTrackTypeTeletextMenu;
            m_tracks[track].emplace_back(av_index, 0, language,
                (static_cast<unsigned>(teletext_magazine_number) << 8) | teletext_page_number);
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Teletext stream #%1 (%2) is in the %3 language on page %4 %5.")
                    .arg(QString::number(i),
                         (teletext_type == 2) ? "Caption" : "Menu",
                         iso639_key_toName(language),
                         QString::number(teletext_magazine_number),
                         QString::number(teletext_page_number)));
        }
    }
}

void AvFormatDecoder::ScanRawTextCaptions(int av_stream_index)
{
    QMutexLocker locker(&m_trackLock);

    AVDictionaryEntry *metatag =
        av_dict_get(m_ic->streams[av_stream_index]->metadata, "language", nullptr,
                    0);
    bool forced = (m_ic->streams[av_stream_index]->disposition & AV_DISPOSITION_FORCED) != 0;
    int lang = metatag ? get_canonical_lang(metatag->value) :
                         iso639_str3_to_key("und");
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Text Subtitle track #%1 is A/V stream #%2 "
                "and is in the %3 language(%4), forced=%5.")
                    .arg(m_tracks[kTrackTypeRawText].size()).arg(av_stream_index)
                    .arg(iso639_key_toName(lang)).arg(lang).arg(forced));
    StreamInfo si {av_stream_index, 0, lang, 0, forced};
    m_tracks[kTrackTypeRawText].push_back(si);
}

/** \fn AvFormatDecoder::ScanDSMCCStreams(AVBufferRef* pmt_section)
 *  \brief Check to see whether there is a Network Boot Ifo sub-descriptor in the PMT which
 *         requires the MHEG application to reboot.
 */
void AvFormatDecoder::ScanDSMCCStreams(AVBufferRef* pmt_section)
{
    if (m_itv == nullptr)
        m_itv = m_parent->GetInteractiveTV();
    if (m_itv == nullptr)
        return;

    MythAVBufferRef pmt_buffer {pmt_section};
    if (!pmt_buffer.has_buffer())
    {
        return;
    }
    const ProgramMapTable pmt(PSIPTable(pmt_buffer.data()));

    for (uint i = 0; i < pmt.StreamCount(); i++)
    {
        if (! StreamID::IsObjectCarousel(pmt.StreamType(i)))
            continue;

        LOG(VB_DSMCC, LOG_NOTICE, QString("ScanDSMCCStreams Found Object Carousel in Stream %1").arg(QString::number(i)));

        const desc_list_t desc_list = MPEGDescriptor::ParseOnlyInclude(
            pmt.StreamInfo(i), pmt.StreamInfoLength(i),
            DescriptorID::data_broadcast_id);

        for (const auto *desc : desc_list)
        {
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
                [[maybe_unused]] uint appTypeCode = desc[0]<<8 | desc[1];
                desc += 3; // Skip app type code and boot priority hint
                uint appSpecDataLen = *desc++;
#if CONFIG_MHEG
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
#endif // CONFIG_MHEG
                {
                    desc += appSpecDataLen;
                }
            }
        }
    }
}

int AvFormatDecoder::autoSelectVideoTrack(int& scanerror)
{
    m_tracks[kTrackTypeVideo].clear();
    m_selectedTrack[kTrackTypeVideo].m_av_stream_index = -1;
    m_currentTrack[kTrackTypeVideo] = -1;
    m_fps = 0;

    const AVCodec *codec = nullptr;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Trying to select best video track");

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
    int stream_index = av_find_best_stream(m_ic, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (stream_index < 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "No video track found/selected.");
        return stream_index;
    }

    AVStream *stream = m_ic->streams[stream_index];
    if (m_averrorCount > SEQ_PKT_ERR_MAX)
        m_codecMap.FreeCodecContext(stream);
    AVCodecContext *codecContext = m_codecMap.GetCodecContext(stream, codec);
    StreamInfo si {stream_index, 0};

    m_tracks[kTrackTypeVideo].push_back(si);
    m_selectedTrack[kTrackTypeVideo] = si;
    m_currentTrack[kTrackTypeVideo] = m_tracks[kTrackTypeVideo].size() - 1;

    QString codectype(AVMediaTypeToString(codecContext->codec_type));
    if (codecContext->codec_type == AVMEDIA_TYPE_VIDEO)
        codectype += QString("(%1x%2)").arg(codecContext->width).arg(codecContext->height);
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Selected track #%1: ID: 0x%2 Codec ID: %3 Profile: %4 Type: %5 Bitrate: %6")
            .arg(stream_index).arg(static_cast<uint64_t>(stream->id), 0, 16)
            .arg(avcodec_get_name(codecContext->codec_id),
                 avcodec_profile_name(codecContext->codec_id, codecContext->profile),
                 codectype,
                 QString::number(codecContext->bit_rate)));

    // If ScanStreams has been called on a stream change triggered by a
    // decoder error - because the decoder does not handle resolution
    // changes gracefully (NVDEC and maybe MediaCodec) - then the stream/codec
    // will still contain the old resolution but the AVCodecContext will
    // have been updated. This causes mayhem for a second or two.
    if ((codecContext->width != stream->codecpar->width) || (codecContext->height != stream->codecpar->height))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString(
            "Video resolution mismatch: Context: %1x%2 Stream: %3x%4 Codec: %5 Stream change: %6")
            .arg(codecContext->width).arg(codecContext->height)
            .arg(stream->codecpar->width).arg(stream->codecpar->height)
            .arg(get_decoder_name(m_videoCodecId)).arg(m_streamsChanged));
    }

    m_avcParser->Reset();

    QSize dim = get_video_dim(*codecContext);
    int width  = std::max(dim.width(),  16);
    int height = std::max(dim.height(), 16);
    QString dec = "ffmpeg";
    uint thread_count = 1;
    QString codecName;
    if (codecContext->codec)
        codecName = codecContext->codec->name;
    // framerate appears to never be set - which is probably why
    // GetVideoFrameRate never uses it:)
    // So fallback to the GetVideoFrameRate call which should then ensure
    // the video display profile gets an accurate frame rate - instead of 0
    if (codecContext->framerate.den && codecContext->framerate.num)
        m_fps = float(codecContext->framerate.num) / float(codecContext->framerate.den);
    else
        m_fps = GetVideoFrameRate(stream, codecContext, true);

    bool foundgpudecoder = false;
    QStringList unavailabledecoders;
    bool allowgpu = FlagIsSet(kDecodeAllowGPU);

    if (m_averrorCount > SEQ_PKT_ERR_MAX)
    {
        // TODO this could be improved by appending the decoder that has
        // failed to the unavailable list - but that could lead to circular
        // failures if there are 2 or more hardware decoders that fail
        if (FlagIsSet(kDecodeAllowGPU) && (dec != "ffmpeg"))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString(
                "GPU/hardware decoder '%1' failed - forcing software decode")
                .arg(dec));
        }
        m_averrorCount = 0;
        allowgpu = false;
    }

    while (unavailabledecoders.size() < 10)
    {
        if (!m_isDbIgnored)
        {
            if (!unavailabledecoders.isEmpty())
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Unavailable decoders: %1")
                    .arg(unavailabledecoders.join(",")));
            }
            m_videoDisplayProfile.SetInput(QSize(width, height), m_fps, codecName, unavailabledecoders);
            dec = m_videoDisplayProfile.GetDecoder();
            thread_count = m_videoDisplayProfile.GetMaxCPUs();
            bool skip_loop_filter = m_videoDisplayProfile.IsSkipLoopEnabled();
            if  (!skip_loop_filter)
                codecContext->skip_loop_filter = AVDISCARD_NONKEY;
        }

        m_videoCodecId = kCodec_NONE;
        uint version = mpeg_version(codecContext->codec_id);
        if (version)
            m_videoCodecId = static_cast<MythCodecID>(kCodec_MPEG1 + version - 1);

        if (version && allowgpu && dec != "ffmpeg")
        {
            // We need to set this so that MythyCodecContext can callback
            // to the player in use to check interop support.
            codecContext->opaque = static_cast<void*>(this);
            MythCodecID hwcodec = MythCodecContext::FindDecoder(dec, stream, &codecContext, &codec);
            if (hwcodec != kCodec_NONE)
            {
                // the context may have changed
                codecContext->opaque = static_cast<void*>(this);
                m_videoCodecId = hwcodec;
                foundgpudecoder = true;
            }
            else
            {
                // hardware decoder is not available - try the next best profile
                unavailabledecoders.append(dec);
                continue;
            }
        }

        // default to mpeg2
        if (m_videoCodecId == kCodec_NONE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown video codec - defaulting to MPEG2");
            m_videoCodecId = kCodec_MPEG2;
        }

        break;
    }

    if (FlagIsSet(kDecodeSingleThreaded))
        thread_count = 1;

    if (HAVE_THREADS)
    {
        // Only use a single thread for hardware decoding. There is no
        // performance improvement with multithreaded hardware decode
        // and asynchronous callbacks create issues with decoders that
        // use AVHWFrameContext where we need to release video resources
        // before they are recreated
        if (!foundgpudecoder)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using %1 CPUs for decoding")
                .arg(HAVE_THREADS ? thread_count : 1));
            codecContext->thread_count = static_cast<int>(thread_count);
        }
    }

    InitVideoCodec(stream, codecContext, true);

    ScanATSCCaptionStreams(stream_index);
    UpdateATSCCaptionTracks();

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using %1 for video decoding").arg(GetCodecDecoderName()));
    m_mythCodecCtx->SetDecoderOptions(codecContext, codec);
    if (!OpenAVCodec(codecContext, codec))
    {
        scanerror = -1;
    }
    return stream_index;
}

void AvFormatDecoder::remove_tracks_not_in_same_AVProgram(int stream_index)
{
    AVProgram* program = av_find_program_from_stream(m_ic, nullptr, stream_index);
    if (program == nullptr)
    {
        return;
    }

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Removing streams not in Program %1 from track selection.")
        .arg(QString::number(program->id)));

    const auto * const begin = program->stream_index;
    const auto * const end   = program->stream_index + program->nb_stream_indexes;

    for (auto & track_list : m_tracks)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG,
            QString("Size before: %1").arg(QString::number(track_list.size())));
        track_list.erase(std::remove_if(track_list.begin(), track_list.end(),
            [&](const StreamInfo& i)
            {
                return std::find(begin, end, i.m_av_stream_index) == end;
            }), track_list.end());
        LOG(VB_PLAYBACK, LOG_DEBUG,
            QString("Size after: %1").arg(QString::number(track_list.size())));
    }
}

static bool is_dual_mono(const AVChannelLayout& ch_layout)
{
    return (ch_layout.order == AV_CHANNEL_ORDER_CUSTOM) &&
           (ch_layout.nb_channels == 2) &&
           (ch_layout.u.map[0].id == AV_CHAN_FRONT_CENTER) &&
           (ch_layout.u.map[1].id == AV_CHAN_FRONT_CENTER);
}

int AvFormatDecoder::ScanStreams(bool novideo)
{
    QMutexLocker avlocker(&m_avCodecLock);
    QMutexLocker locker(&m_trackLock);

    bool unknownbitrate = false;
    int scanerror = 0;
    m_bitrate     = 0;

    constexpr std::array<TrackType, 6> types {
        kTrackTypeAttachment,
        kTrackTypeAudio,
        kTrackTypeSubtitle,
        kTrackTypeTeletextCaptions,
        kTrackTypeTeletextMenu,
        kTrackTypeRawText,
        };
    for (const auto type : types)
    {
        m_tracks[type].clear();
        m_currentTrack[type] = -1;
    }

    std::map<int,uint> lang_sub_cnt;
    uint subtitleStreamCount = 0;
    std::map<int,uint> lang_aud_cnt;
    uint audioStreamCount = 0;

    if (m_ringBuffer && m_ringBuffer->IsDVD() &&
        m_ringBuffer->DVD()->AudioStreamsChanged())
    {
        m_ringBuffer->DVD()->AudioStreamsChanged(false);
        RemoveAudioStreams();
    }

    if (m_ic == nullptr)
        return -1;

    for (uint strm = 0; strm < m_ic->nb_streams; strm++)
    {
        AVCodecParameters *par = m_ic->streams[strm]->codecpar;

        QString codectype(AVMediaTypeToString(par->codec_type));
        if (par->codec_type == AVMEDIA_TYPE_VIDEO)
            codectype += QString("(%1x%2)").arg(par->width).arg(par->height);
        QString program_id = "null";
        if (av_find_program_from_stream(m_ic, nullptr, strm) != nullptr)
        {
            program_id = QString::number(av_find_program_from_stream(m_ic, nullptr, strm)->id);
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stream #%1: ID: 0x%2 Program ID: %3 Codec ID: %4 Type: %5 Bitrate: %6").arg(
                QString::number(strm),
                QString::number(static_cast<uint64_t>(m_ic->streams[strm]->id), 16),
                program_id,
                avcodec_get_name(par->codec_id),
                codectype,
                QString::number(par->bit_rate))
            );

        switch (par->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                // reset any potentially errored hardware decoders
                if (m_resetHardwareDecoders)
                {
                    if (m_codecMap.FindCodecContext(m_ic->streams[strm]))
                    {
                        AVCodecContext* ctx = m_codecMap.GetCodecContext(m_ic->streams[strm]);
                        if (ctx && (ctx->hw_frames_ctx || ctx->hw_device_ctx))
                            m_codecMap.FreeCodecContext(m_ic->streams[strm]);
                    }
                }

                if (!par->codec_id)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC +
                        QString("Stream #%1 has an unknown video "
                                "codec id, skipping.").arg(strm));
                    continue;
                }

                // ffmpeg does not return a bitrate for several codecs and
                // formats. Typically the same streams do not have a duration either
                // - so we cannot estimate a bitrate (which would be subject
                // to significant error anyway if there were multiple video streams).
                // So we need to guesstimate a value that avoids low bitrate optimisations
                // (which typically kick in around 500,000) and provides a read
                // chunk size large enough to avoid starving the decoder of data.
                // Trying to read a 20Mbs stream with a 16KB chunk size does not work:)
                if (par->bit_rate == 0)
                {
                    static constexpr int64_t s_baseBitrate { 1000000LL };
                    int multiplier = 1;
                    if (par->width && par->height)
                    {
                        static const int s_baseSize = 1920 * 1080;
                        multiplier = ((par->width * par->height) + s_baseSize - 1) / s_baseSize;
                        multiplier = std::max(multiplier, 1);
                    }
                    par->bit_rate = s_baseBitrate * multiplier;
                    unknownbitrate = true;
                }
                m_bitrate += par->bit_rate;

                break;
            }
            case AVMEDIA_TYPE_AUDIO:
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    QString("codec %1 has %2 channels")
                        .arg(avcodec_get_name(par->codec_id))
                        .arg(par->ch_layout.nb_channels));

                m_bitrate += par->bit_rate;
                break;
            }
            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (par->codec_id == AV_CODEC_ID_DVB_TELETEXT)
                    ScanTeletextCaptions(static_cast<int>(strm));
                if (par->codec_id == AV_CODEC_ID_TEXT)
                    ScanRawTextCaptions(static_cast<int>(strm));
                m_bitrate += par->bit_rate;

                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("subtitle codec (%1)")
                        .arg(AVMediaTypeToString(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_DATA:
            {
                if (par->codec_id == AV_CODEC_ID_DVB_VBI)
                    ScanTeletextCaptions(static_cast<int>(strm));
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("data codec (%1)")
                        .arg(AVMediaTypeToString(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_ATTACHMENT:
            {
                if (par->codec_id == AV_CODEC_ID_TTF)
                   m_tracks[kTrackTypeAttachment].emplace_back(static_cast<int>(strm), m_ic->streams[strm]->id);
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Attachment codec (%1)")
                        .arg(AVMediaTypeToString(par->codec_type)));
                break;
            }
            default:
            {
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("Unknown codec type (%1)")
                        .arg(AVMediaTypeToString(par->codec_type)));
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
                .arg(avcodec_get_name(par->codec_id)));

        if (par->codec_id == AV_CODEC_ID_PROBE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Probing of stream #%1 unsuccesful, ignoring.").arg(strm));
            continue;
        }

        AVCodecContext* codecContext = m_codecMap.GetCodecContext(m_ic->streams[strm]);

        if (codecContext == nullptr)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Could not find decoder for codec (%1), ignoring.")
                    .arg(avcodec_get_name(par->codec_id)));
            LOG(VB_LIBAV, LOG_INFO, "For a list of all codecs, run `mythffmpeg -codecs`.");
            continue;
        }

        if (codecContext->codec && par->codec_id != codecContext->codec_id)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Already opened codec not matching (%1 vs %2). Reopening")
                .arg(avcodec_get_name(codecContext->codec_id),
                     avcodec_get_name(codecContext->codec->id)));
            m_codecMap.FreeCodecContext(m_ic->streams[strm]);
            codecContext = m_codecMap.GetCodecContext(m_ic->streams[strm]);
        }
        if (!OpenAVCodec(codecContext, codecContext->codec))
            continue;
        if (!codecContext)
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
            uint lang_indx = lang_sub_cnt[lang]++;
            subtitleStreamCount++;

            m_tracks[kTrackTypeSubtitle].emplace_back(
                static_cast<int>(strm), m_ic->streams[strm]->id, lang, lang_indx, forced);

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
            uint lang_indx = lang_aud_cnt[lang]++;
            audioStreamCount++;

            int stream_id = m_ic->streams[strm]->id;
            if (m_ringBuffer && m_ringBuffer->IsDVD())
            {
                stream_id = m_ringBuffer->DVD()->GetAudioTrackNum(stream_id);
                if (stream_id == -1)
                {
                    // This stream isn't mapped, so skip it
                    continue;
                }
            }

            m_tracks[kTrackTypeAudio].emplace_back(
                static_cast<int>(strm), stream_id, lang, lang_indx, type);

            if (is_dual_mono(codecContext->ch_layout))
            {
                lang_indx = lang_aud_cnt[lang]++;
                m_tracks[kTrackTypeAudio].emplace_back(
                    static_cast<int>(strm), stream_id, lang, lang_indx, type);
            }

            LOG(VB_AUDIO, LOG_INFO, LOC +
                QString("Audio Track #%1, of type (%2) is A/V stream #%3 (id=0x%4) "
                        "and has %5 channels in the %6 language(%7).")
                    .arg(m_tracks[kTrackTypeAudio].size()).arg(toString(type))
                    .arg(strm).arg(m_ic->streams[strm]->id,0,16).arg(codecContext->ch_layout.nb_channels)
                    .arg(iso639_key_toName(lang)).arg(lang));
        }
    }

    // Now find best video track to play
    m_resetHardwareDecoders = false;
    if (!novideo)
    {
        int stream_index = autoSelectVideoTrack(scanerror);
        remove_tracks_not_in_same_AVProgram(stream_index);
    }

    m_bitrate = std::max(static_cast<uint>(m_ic->bit_rate), m_bitrate);

    if (m_bitrate > 0)
    {
        m_bitrate = (m_bitrate + 999) / 1000;
        if (m_ringBuffer)
            m_ringBuffer->UpdateRawBitrate(m_bitrate);
    }

    // update RingBuffer buffer size
    if (m_ringBuffer)
    {
        m_ringBuffer->SetBufferSizeFactors(unknownbitrate,
                        QString(m_ic->iformat->name).contains("matroska"));
    }

    PostProcessTracks();

    for (const auto type : types)
    {
        AutoSelectTrack(type);
    }

    // We have to do this here to avoid the NVP getting stuck
    // waiting on audio.
    if (m_audio->HasAudioIn() && m_tracks[kTrackTypeAudio].empty())
    {
        m_audio->SetAudioParams(FORMAT_NONE, -1, -1, AV_CODEC_ID_NONE, -1, false);
        m_audio->ReinitAudio();
        if (m_ringBuffer && m_ringBuffer->IsDVD())
            m_audioIn = AudioInfo();
    }

    // if we don't have a video stream we still need to make sure some
    // video params are set properly
    if (m_selectedTrack[kTrackTypeVideo].m_av_stream_index == -1)
    {
        m_fps = 23.97F; // minimum likey display refresh rate
        // N.B. we know longer need a 'dummy' frame to render overlays into
        m_parent->SetVideoParams(0, 0, 23.97, 1.0F, false, 0);
    }

    if (m_parent->IsErrored())
        scanerror = -1;

    if (!novideo && m_selectedTrack[kTrackTypeVideo].m_av_stream_index != -1)
    {
        ScanDSMCCStreams(
            get_pmt_section_for_AVStream_index(m_ic, m_selectedTrack[kTrackTypeVideo].m_av_stream_index));
    }
    else
    {
        /* We don't yet know which AVProgram we want, so iterate through them
        all.  This should be no more incorrect than using the last PMT when
        there are multiple programs. */
        for (unsigned i = 0; i < m_ic->nb_programs; i++)
        {
            ScanDSMCCStreams(m_ic->programs[i]->pmt_section);
        }
    }

    return scanerror;
}

bool AvFormatDecoder::OpenAVCodec(AVCodecContext *avctx, const AVCodec *codec)
{
    m_avCodecLock.lock();
#if CONFIG_MEDIACODEC
    if (QString("mediacodec") == codec->wrapper_name)
        av_jni_set_java_vm(QAndroidJniEnvironment::javaVM(), nullptr);
#endif
    int ret = avcodec_open2(avctx, codec, nullptr);
    m_avCodecLock.unlock();
    if (ret < 0)
    {
        std::string error;
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not open codec 0x%1, id(%2) type(%3) "
                    "ignoring. reason %4").arg((uint64_t)avctx,0,16)
            .arg(avcodec_get_name(avctx->codec_id),
                 AVMediaTypeToString(avctx->codec_type),
                 av_make_error_stdstring(error, ret)));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Opened codec 0x%1, id(%2) type(%3)")
        .arg((uint64_t)avctx,0,16)
        .arg(avcodec_get_name(avctx->codec_id),
             AVMediaTypeToString(avctx->codec_type)));
    return true;
}

void AvFormatDecoder::UpdateFramesPlayed(void)
{
    DecoderBase::UpdateFramesPlayed();
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
int AvFormatDecoder::GetTeletextLanguage(uint Index)
{
    QMutexLocker locker(&m_trackLock);
    for (const auto & si : m_tracks[kTrackTypeTeletextCaptions])
        if (si.m_language_index == Index)
             return si.m_language;
    return iso639_str3_to_key("und");
}

/// Returns DVD Subtitle language
int AvFormatDecoder::GetSubtitleLanguage(uint /*unused*/, uint StreamIndex)
{
    AVDictionaryEntry *metatag = av_dict_get(m_ic->streams[StreamIndex]->metadata, "language", nullptr, 0);
    return metatag ? get_canonical_lang(metatag->value) : iso639_str3_to_key("und");
}

/// Return ATSC Closed Caption Language
int AvFormatDecoder::GetCaptionLanguage(TrackType TrackType, int ServiceNum)
{
    // This doesn't strictly need write lock but it is called internally while
    // write lock is held. All other (external) uses are safe
    QMutexLocker locker(&m_trackLock);

    int ret = -1;
    for (int i = 0; i < m_pmtTrackTypes.size(); i++)
    {
        if ((m_pmtTrackTypes[i] == TrackType) && (m_pmtTracks[i].m_stream_id == ServiceNum))
        {
            ret = m_pmtTracks[i].m_language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    for (int i = 0; i < m_streamTrackTypes.size(); i++)
    {
        if ((m_streamTrackTypes[i] == TrackType) && (m_streamTracks[i].m_stream_id == ServiceNum))
        {
            ret = m_streamTracks[i].m_language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    return ret;
}

int AvFormatDecoder::GetAudioLanguage(uint AudioIndex, uint StreamIndex)
{
    return GetSubtitleLanguage(AudioIndex, StreamIndex);
}

AudioTrackType AvFormatDecoder::GetAudioTrackType(uint StreamIndex)
{
    AudioTrackType type = kAudioTypeNormal;
    AVStream *stream = m_ic->streams[StreamIndex];

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
    QMutexLocker locker(&m_trackLock);

    // Find the position of the streaminfo in m_tracks[kTrackTypeAudio]
    auto current = m_tracks[kTrackTypeAudio].begin();
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
    auto next = current + 1;
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

    m_avCodecLock.lock();
    bool do_flush = false;
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *st = m_ic->streams[i];
        st->index = i;
        AVCodecContext *avctx = m_codecMap.FindCodecContext(st);
        if (avctx && avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_codecMap.FreeCodecContext(st);
            LOG(VB_LIBAV, LOG_DEBUG, QString("removing audio stream (id: 0x%1, index: %2, nb_streams: %3)")
                .arg(QString::number(st->id, 16),
                    QString::number(i),
                    QString::number(m_ic->nb_streams)
                    )
                );
            m_ic->nb_streams--;
            if ((m_ic->nb_streams - i) > 0) {
                std::memmove(reinterpret_cast<void*>(&m_ic->streams[i]),
                             reinterpret_cast<const void*>(&m_ic->streams[i + 1]),
                             (m_ic->nb_streams - i) * sizeof(AVFormatContext*));
            }
            else
            {
                m_ic->streams[i] = nullptr;
            }
            do_flush = true;
            i--;
        }
    }
    if (do_flush)
    {
        avformat_flush(m_ic);
    }
    m_avCodecLock.unlock();
}

int get_avf_buffer(struct AVCodecContext *c, AVFrame *pic, int flags)
{
    auto *decoder = static_cast<AvFormatDecoder*>(c->opaque);
    VideoFrameType type = MythAVUtil::PixelFormatToFrameType(c->pix_fmt);
    if (!std::any_of(decoder->m_renderFormats->cbegin(), decoder->m_renderFormats->cend(),
                     [&type](auto Format) { return type == Format; }))
    {
        decoder->m_directRendering = false;
        return avcodec_default_get_buffer2(c, pic, flags);
    }

    decoder->m_directRendering = true;
    MythVideoFrame *frame = decoder->GetPlayer()->GetNextVideoFrame();
    if (!frame)
        return -1;

    // We pre-allocate frames to certain alignments. If the coded size differs from
    // those alignments then re-allocate the frame. Changes in frame type (e.g.
    // YV12 to NV12) are always reallocated.
    int width  = (frame->m_width + MYTH_WIDTH_ALIGNMENT - 1) & ~(MYTH_WIDTH_ALIGNMENT - 1);
    int height = (frame->m_height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT - 1);

    if ((frame->m_type != type) || (pic->width > width) || (pic->height > height))
    {
        if (!VideoBuffers::ReinitBuffer(frame, type, decoder->m_videoCodecId, pic->width, pic->height))
            return -1;
        // NB the video frame may now have a new size which is currenly not an issue
        // as the underlying size has already been passed through to the player.
        // But may cause issues down the line. We should add coded_width/height to
        // VideoFrame as well to ensure consistentency through the rendering
        // pipeline.
        // NB remember to compare coded width/height - not 'true' values - otherwsie
        // we continually re-allocate the frame.
        //frame->width = c->width;
        //frame->height = c->height;
    }

    frame->m_colorshifted = false;
    uint max = MythVideoFrame::GetNumPlanes(frame->m_type);
    for (uint i = 0; i < 3; i++)
    {
        pic->data[i]     = (i < max) ? (frame->m_buffer + frame->m_offsets[i]) : nullptr;
        pic->linesize[i] = frame->m_pitches[i];
    }

    pic->opaque = frame;

    // Set release method
    AVBufferRef *buffer = av_buffer_create(reinterpret_cast<uint8_t*>(frame), 0,
                            [](void* Opaque, uint8_t* Data)
                                {
                                    auto *avfd = static_cast<AvFormatDecoder*>(Opaque);
                                    auto *vf = reinterpret_cast<MythVideoFrame*>(Data);
                                    if (avfd && avfd->GetPlayer())
                                        avfd->GetPlayer()->DeLimboFrame(vf);
                                }
                            , decoder, 0);
    pic->buf[0] = buffer;

    return 0;
}

#if CONFIG_DXVA2
int get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic, int /*flags*/)
{
    AvFormatDecoder *nd = (AvFormatDecoder *)(c->opaque);
    MythVideoFrame *frame = nd->GetPlayer()->GetNextVideoFrame();

    for (int i = 0; i < 4; i++)
    {
        pic->data[i]     = nullptr;
        pic->linesize[i] = 0;
    }
    pic->opaque      = frame;
    frame->m_pixFmt  = c->pix_fmt;
    pic->data[0] = (uint8_t*)frame->m_buffer;
    pic->data[3] = (uint8_t*)frame->m_buffer;

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)frame, 0,
                         [](void* Opaque, uint8_t* Data)
                             {
                                 AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Opaque);
                                 MythVideoFrame *vf = reinterpret_cast<MythVideoFrame*>(Data);
                                 if (avfd && avfd->GetPlayer())
                                     avfd->GetPlayer()->DeLimboFrame(vf);
                             }
                         , nd, 0);
    pic->buf[0] = buffer;

    return 0;
}
#endif

void AvFormatDecoder::DecodeCCx08(const uint8_t *buf, uint buf_size)
{
    if (buf_size < 3)
        return;

    bool had_608 = false;
    bool had_708 = false;
    for (uint cur = 0; cur + 2 < buf_size; cur += 3)
    {
        uint cc_code  = buf[cur];
        bool cc_valid = (cc_code & 0x04) != 0U;

        uint data1    = buf[cur+1];
        uint data2    = buf[cur+2];
        uint data     = (data2 << 8) | data1;
        uint cc_type  = cc_code & 0x03;

        if (!cc_valid)
        {
            continue;
        }

        if (cc_type <= 0x1) // EIA-608 field-1/2
        {
            if (cc608_good_parity(data))
            {
                had_608 = true;
                m_ccd608->FormatCCField(duration_cast<std::chrono::milliseconds>(m_lastCcPtsu), cc_type, data);
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
    CC608Seen seen_608;
    if (check_608)
    {
        m_ccd608->GetServices(15s, seen_608);
        for (uint i = 0; i < 4; i++)
        {
            need_change_608 |= (seen_608[i] && !m_ccX08InTracks[i]) ||
                (!seen_608[i] && m_ccX08InTracks[i] && !m_ccX08InPmt[i]);
        }
    }

    bool need_change_708 = false;
    cc708_seen_flags seen_708;
    if (check_708 || need_change_608)
    {
        m_ccd708->services(15s, seen_708);
        for (uint i = 1; i < 64 && !need_change_608 && !need_change_708; i++)
        {
            need_change_708 |= (seen_708[i] && !m_ccX08InTracks[i+4]) ||
                (!seen_708[i] && m_ccX08InTracks[i+4] && !m_ccX08InPmt[i+4]);
        }
        if (need_change_708 && !check_608)
            m_ccd608->GetServices(15s, seen_608);
    }

    if (!need_change_608 && !need_change_708)
        return;

    m_trackLock.lock();

    ScanATSCCaptionStreams(m_selectedTrack[kTrackTypeVideo].m_av_stream_index);

    m_streamTracks.clear();
    m_streamTrackTypes.clear();
    int av_index = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    int lang = iso639_str3_to_key("und");
    for (int i = 1; i < 64; i++)
    {
        if (seen_708[i] && !m_ccX08InPmt[i+4])
        {
            StreamInfo si {av_index, i, lang};
            m_streamTracks.push_back(si);
            m_streamTrackTypes.push_back(kTrackTypeCC708);
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (seen_608[i] && !m_ccX08InPmt[i])
        {
            if (0==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 1);
            else if (2==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 2);
            else
                lang = iso639_str3_to_key("und");

            StreamInfo si {av_index, i+1, lang};
            m_streamTracks.push_back(si);
            m_streamTrackTypes.push_back(kTrackTypeCC608);
        }
    }
    m_trackLock.unlock();
    UpdateATSCCaptionTracks();
}

void AvFormatDecoder::HandleGopStart(
    AVPacket *pkt, bool can_reliably_parse_keyframes)
{
    if (m_prevGopPos != 0 && m_keyframeDist != 1)
    {
        int tempKeyFrameDist = m_framesRead - 1 - m_prevGopPos;
        bool reset_kfd = false;

        if (!m_gopSet || m_livetv) // gopset: we've seen 2 keyframes
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "gopset not set, syncing positionMap");
            SyncPositionMap();
            if (tempKeyFrameDist > 0 && !m_livetv)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Initial key frame distance: %1.")
                        .arg(m_keyframeDist));
                m_gopSet     = true;
                reset_kfd    = true;
            }
        }
        else if (m_keyframeDist != tempKeyFrameDist && tempKeyFrameDist > 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Key frame distance changed from %1 to %2.")
                    .arg(m_keyframeDist).arg(tempKeyFrameDist));
            reset_kfd = true;
        }

        if (reset_kfd)
        {
            m_keyframeDist    = tempKeyFrameDist;
            m_maxKeyframeDist = std::max(m_keyframeDist, m_maxKeyframeDist);

            m_parent->SetKeyframeDistance(m_keyframeDist);

#if 0
            // also reset length
            QMutexLocker locker(&m_positionMapLock);
            if (!m_positionMap.empty())
            {
                long long index       = m_positionMap.back().index;
                long long totframes   = index * m_keyframeDist;
                uint length = (uint)((totframes * 1.0F) / m_fps);
                m_parent->SetFileLength(length, totframes);
            }
#endif
        }
    }

    m_lastKey = m_prevGopPos = m_framesRead - 1;

    if (can_reliably_parse_keyframes &&
        !m_hasFullPositionMap && !m_livetv && !m_watchingRecording)
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
                .arg(m_framesRead) .arg(last_frame) .arg(m_keyframeDist));
#endif

        // if we don't have an entry, fill it in with what we've just parsed
        if (m_framesRead > last_frame && m_keyframeDist > 0)
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
                long long duration = m_totalDuration.toFixed(1000LL);
                m_frameToDurMap[m_framesRead] = duration;
                m_durToFrameMap[duration]     = m_framesRead;
            }
        }

#if 0
        // If we are > 150 frames in and saw no positionmap at all, reset
        // length based on the actual bitrate seen so far
        if (m_framesRead > 150 && !m_recordingHasPositionMap && !m_livetv)
        {
            m_bitrate = (int)((pkt->pos * 8 * m_fps) / (m_framesRead - 1));
            float bytespersec = (float)m_bitrate / 8;
            float secs = m_ringBuffer->GetRealFileSize() * 1.0F / bytespersec;
            m_parent->SetFileLength((int)(secs), (int)(secs * m_fps));
        }
#endif
    }
}

static constexpr uint32_t SEQ_START       { 0x000001b3 };
static constexpr uint32_t GOP_START       { 0x000001b8 };
//static constexpr uint32_t PICTURE_START { 0x00000100 };
static constexpr uint32_t SLICE_MIN       { 0x00000101 };
static constexpr uint32_t SLICE_MAX       { 0x000001af };
//static constexpr uint32_t SEQ_END_CODE  { 0x000001b7 };

void AvFormatDecoder::MpegPreProcessPkt(AVCodecContext* context, AVStream *stream, AVPacket *pkt)
{
    const uint8_t *bufptr = pkt->data;
    const uint8_t *bufend = pkt->data + pkt->size;

    while (bufptr < bufend)
    {
        bufptr = ByteReader::find_start_code_truncated(bufptr, bufend, &m_startCodeState);

        float aspect_override = -1.0F;
        if (m_ringBuffer->IsDVD())
            aspect_override = m_ringBuffer->DVD()->GetAspectOverride();

        if (m_startCodeState >= SLICE_MIN && m_startCodeState <= SLICE_MAX)
            continue;

        if (SEQ_START == m_startCodeState)
        {
            if (bufptr + 11 >= pkt->data + pkt->size)
                continue; // not enough valid data...
            const auto *seq = reinterpret_cast<const SequenceHeader*>(bufptr);

            int  width  = static_cast<int>(seq->width())  >> context->lowres;
            int  height = static_cast<int>(seq->height()) >> context->lowres;
            float aspect = seq->aspect(context->codec_id == AV_CODEC_ID_MPEG1VIDEO);
            if (stream->sample_aspect_ratio.num)
                aspect = static_cast<float>(av_q2d(stream->sample_aspect_ratio) * width / height);
            if (aspect_override > 0.0F)
                aspect = aspect_override;
            float seqFPS = seq->fps();

            bool changed = (width  != m_currentWidth );
            changed     |= (height != m_currentHeight);
            changed     |= (seqFPS > static_cast<float>(m_fps)+0.01F) ||
                           (seqFPS < static_cast<float>(m_fps)-0.01F);

            // some hardware decoders (e.g. VAAPI MPEG2) will reset when the aspect
            // ratio changes
            bool forceaspectchange = !qFuzzyCompare(m_currentAspect + 10.0F, aspect + 10.0F) &&
                                      m_mythCodecCtx && m_mythCodecCtx->DecoderWillResetOnAspect();
            m_currentAspect = aspect;

            if (changed || forceaspectchange)
            {
                // N.B. We now set the default scan to kScan_Ignore as interlaced detection based on frame
                // size and rate is extremely error prone and FFmpeg gets it right far more often.
                // As for H.264, if a decoder deinterlacer is in operation - the stream must be progressive
                bool doublerate = false;
                bool decoderdeint = m_mythCodecCtx && m_mythCodecCtx->IsDeinterlacing(doublerate, true);
                m_parent->SetVideoParams(width, height, static_cast<double>(seqFPS), m_currentAspect,
                                         forceaspectchange, 2,
                                         decoderdeint ? kScan_Progressive : kScan_Ignore);

                if (context->hw_frames_ctx)
                    if (context->internal)
                        avcodec_flush_buffers(context);

                m_currentWidth   = width;
                m_currentHeight  = height;
                m_fps            = seqFPS;

                m_gopSet = false;
                m_prevGopPos = 0;
                m_firstVPts = m_lastAPts = m_lastVPts = 0ms;
                m_lastCcPtsu = 0us;
                m_firstVPtsInuse = true;

                // fps debugging info
                float avFPS = GetVideoFrameRate(stream, context);
                if ((seqFPS > avFPS+0.01F) || (seqFPS < avFPS-0.01F))
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("avFPS(%1) != seqFPS(%2)")
                            .arg(static_cast<double>(avFPS)).arg(static_cast<double>(seqFPS)));
                }
            }

            m_seqCount++;

            if (!m_seenGop && m_seqCount > 1)
            {
                HandleGopStart(pkt, true);
                pkt->flags |= AV_PKT_FLAG_KEY;
            }
        }
        else if (GOP_START == m_startCodeState)
        {
            HandleGopStart(pkt, true);
            m_seenGop = true;
            pkt->flags |= AV_PKT_FLAG_KEY;
        }
    }
}

// Returns the number of frame starts identified in the packet.
int AvFormatDecoder::H264PreProcessPkt(AVCodecContext* context, AVStream *stream, AVPacket *pkt)
{
    const uint8_t  *buf     = pkt->data;
    const uint8_t  *buf_end = pkt->data + pkt->size;
    int num_frames = 0;

    // The parser only understands Annex B/bytestream format - so check for avCC
    // format (starts with 0x01) and rely on FFmpeg keyframe detection
    if (context->extradata && (context->extradata_size >= 7) && (context->extradata[0] == 0x01))
    {
        if (pkt->flags & AV_PKT_FLAG_KEY)
            HandleGopStart(pkt, false);
        return 1;
    }

    while (buf < buf_end)
    {
        buf += m_avcParser->addBytes(buf, static_cast<unsigned int>(buf_end - buf), 0);

        if (m_avcParser->stateChanged())
        {
            if (m_avcParser->getFieldType() != AVCParser::FIELD_BOTTOM)
            {
                if (m_avcParser->onFrameStart())
                    ++num_frames;

                if (!m_avcParser->onKeyFrameStart())
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

        m_nextDecodedFrameIsKeyFrame = true;
        float aspect = get_aspect(*m_avcParser);
        int width  = static_cast<int>(m_avcParser->pictureWidthCropped());
        int height = static_cast<int>(m_avcParser->pictureHeightCropped());
        double seqFPS = m_avcParser->frameRate();

        bool res_changed = ((width  != m_currentWidth) || (height != m_currentHeight));
        bool fps_changed = (seqFPS > 0.0) && ((seqFPS > m_fps + 0.01) ||
                                              (seqFPS < m_fps - 0.01));
        bool forcechange = !qFuzzyCompare(aspect + 10.0F, m_currentAspect) &&
                            m_mythCodecCtx && m_mythCodecCtx->DecoderWillResetOnAspect();
        m_currentAspect = aspect;

        if (fps_changed || res_changed || forcechange)
        {
            // N.B. we now set the default scan to kScan_Ignore as interlaced detection based on frame
            // size and rate is extremely error prone and FFmpeg gets it right far more often.
            // N.B. if a decoder deinterlacer is in use - the stream must be progressive
            bool doublerate = false;
            bool decoderdeint = m_mythCodecCtx ? m_mythCodecCtx->IsDeinterlacing(doublerate, true) : false;
            m_parent->SetVideoParams(width, height, seqFPS, m_currentAspect, forcechange,
                                     static_cast<int>(m_avcParser->getRefFrames()),
                                     decoderdeint ? kScan_Progressive : kScan_Ignore);

            // the SetVideoParams call above will have released all held frames
            // when using a hardware frames context - but for H264 (as opposed to mpeg2)
            // the codec will still hold references for reference frames (decoded picture buffer).
            // Flushing the context will release these references and the old
            // hardware context is released correctly before a new one is created.
            // TODO check what is needed here when a device context is used
            // TODO check whether any codecs need to be flushed for a frame rate change (e.g. mediacodec?)
            if (context->hw_frames_ctx && (forcechange || res_changed))
                if (context->internal)
                    avcodec_flush_buffers(context);

            m_currentWidth  = width;
            m_currentHeight = height;

            if (seqFPS > 0.0)
                m_fps = static_cast<float>(seqFPS);

            m_gopSet = false;
            m_prevGopPos = 0;
            m_firstVPts = m_lastAPts = m_lastVPts = 0ms;
            m_lastCcPtsu = 0us;
            m_firstVPtsInuse = true;

            // fps debugging info
            auto avFPS = static_cast<double>(GetVideoFrameRate(stream, context));
            if ((seqFPS > avFPS + 0.01) || (seqFPS < avFPS - 0.01))
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("avFPS(%1) != seqFPS(%2)").arg(avFPS).arg(seqFPS));
            }
        }

        HandleGopStart(pkt, true);
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    return num_frames;
}

bool AvFormatDecoder::PreProcessVideoPacket(AVCodecContext* context, AVStream *curstream, AVPacket *pkt)
{
    int num_frames = 1;

    if (CODEC_IS_MPEG(context->codec_id))
    {
        MpegPreProcessPkt(context, curstream, pkt);
    }
    else if (CODEC_IS_H264(context->codec_id))
    {
        num_frames = H264PreProcessPkt(context, curstream, pkt);
    }
    else
    {
        if (pkt->flags & AV_PKT_FLAG_KEY)
        {
            HandleGopStart(pkt, false);
            m_seenGop = true;
        }
        else
        {
            m_seqCount++;
            if (!m_seenGop && m_seqCount > 1)
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
        MythAVRational pkt_dur {static_cast<int>(pkt->duration)};
        pkt_dur *= MythAVRational(curstream->time_base);
        if (pkt_dur == MythAVRational(1501, 90000))
            pkt_dur = MythAVRational(1001, 60000); // 1501.5/90000
        m_totalDuration += pkt_dur;
    }

    m_justAfterChange = false;

    if (m_exitAfterDecoded)
        m_gotVideoFrame = true;

    return true;
}

bool AvFormatDecoder::ProcessVideoPacket(AVCodecContext* context, AVStream *curstream, AVPacket *pkt, bool &Retry)
{
    int ret = 0;
    int gotpicture = 0;
    MythAVFrame mpa_pic;
    if (!mpa_pic)
        return false;

    bool sentPacket = false;
    int ret2 = 0;

    m_avCodecLock.lock();

    //  SUGGESTION
    //  Now that avcodec_decode_video2 is deprecated and replaced
    //  by 2 calls (receive frame and send packet), this could be optimized
    //  into separate routines or separate threads.
    //  Also now that it always consumes a whole buffer some code
    //  in the caller may be able to be optimized.

    // FilteredReceiveFrame will call avcodec_receive_frame and
    // apply any codec-dependent filtering
    ret = m_mythCodecCtx->FilteredReceiveFrame(context, mpa_pic);

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
            Retry = true;
            ret2 = 0;
        }
        else
        {
            sentPacket = true;
        }
    }
    m_avCodecLock.unlock();

    if (ret < 0 || ret2 < 0)
    {
        std::string error;
        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("video avcodec_receive_frame error: %1 (%2) gotpicture:%3")
                .arg(av_make_error_stdstring(error, ret))
                .arg(ret).arg(gotpicture));
        }

        if (ret2 < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("video avcodec_send_packet error: %1 (%2) gotpicture:%3")
                .arg(av_make_error_stdstring(error, ret2))
                .arg(ret2).arg(gotpicture));
        }

        if (++m_averrorCount > SEQ_PKT_ERR_MAX)
        {
            // If erroring on GPU assist, try switching to software decode
            if (codec_is_std(m_videoCodecId))
                m_parent->SetErrored(QObject::tr("Video Decode Error"));
            else
                m_streamsChanged = true;
        }

        if (m_mythCodecCtx->DecoderNeedsReset(context))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Decoder needs reset");
            m_resetHardwareDecoders = m_streamsChanged = true;
        }

        if (ret == AVERROR_EXTERNAL || ret2 == AVERROR_EXTERNAL)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "FFmpeg external library error - assuming streams changed");
            m_streamsChanged = true;
        }

        return false;
    }

    // averror_count counts sequential errors, so if you have a successful
    // packet then reset it
    m_averrorCount = 0;
    if (gotpicture)
    {
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("video timecodes packet-pts:%1 frame-pts:%2 packet-dts: %3 frame-dts:%4")
                .arg(pkt->pts).arg(mpa_pic->pts).arg(pkt->dts)
                .arg(mpa_pic->pkt_dts));

        ProcessVideoFrame(context, curstream, mpa_pic);
    }

    if (!sentPacket)
    {
        // MythTV logic expects that only one frame is processed
        // Save the packet for later and return.
        auto *newPkt = av_packet_clone(pkt);
        if (newPkt)
            m_storedPackets.prepend(newPkt);
    }
    return true;
}

bool AvFormatDecoder::ProcessVideoFrame(AVCodecContext* context, AVStream *Stream, AVFrame *AvFrame)
{
    // look for A53 captions
    auto * side_data = av_frame_get_side_data(AvFrame, AV_FRAME_DATA_A53_CC);
    if (side_data && (side_data->size > 0))
        DecodeCCx08(side_data->data, static_cast<uint>(side_data->size));

    auto * frame = static_cast<MythVideoFrame*>(AvFrame->opaque);
    if (frame)
        frame->m_directRendering = m_directRendering;

    if (FlagIsSet(kDecodeNoDecode))
    {
        // Do nothing, we just want the pts, captions, subtites, etc.
        // So we can release the unconverted blank video frame to the
        // display queue.
        if (frame)
            frame->m_directRendering = false;
    }
    else if (!m_directRendering)
    {
        MythVideoFrame *oldframe = frame;
        frame = m_parent->GetNextVideoFrame();
        frame->m_directRendering = false;

        if (!m_mythCodecCtx->RetrieveFrame(context, frame, AvFrame))
        {
            AVFrame tmppicture;
            av_image_fill_arrays(tmppicture.data, tmppicture.linesize,
                                 frame->m_buffer, AV_PIX_FMT_YUV420P, AvFrame->width,
                                 AvFrame->height, IMAGE_ALIGN);
            tmppicture.data[0] = frame->m_buffer + frame->m_offsets[0];
            tmppicture.data[1] = frame->m_buffer + frame->m_offsets[1];
            tmppicture.data[2] = frame->m_buffer + frame->m_offsets[2];
            tmppicture.linesize[0] = frame->m_pitches[0];
            tmppicture.linesize[1] = frame->m_pitches[1];
            tmppicture.linesize[2] = frame->m_pitches[2];

            QSize dim = get_video_dim(*context);
            m_swsCtx = sws_getCachedContext(m_swsCtx, AvFrame->width,
                                        AvFrame->height, static_cast<AVPixelFormat>(AvFrame->format),
                                        AvFrame->width, AvFrame->height,
                                        AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                                        nullptr, nullptr, nullptr);
            if (!m_swsCtx)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate sws context");
                return false;
            }
            sws_scale(m_swsCtx, AvFrame->data, AvFrame->linesize, 0, dim.height(),
                      tmppicture.data, tmppicture.linesize);
        }

        // Discard any old VideoFrames
        if (oldframe)
        {
            // Set the frame flags, but then discard it
            // since we are not using it for display.
            oldframe->m_interlaced     = (AvFrame->flags & AV_FRAME_FLAG_INTERLACED) != 0;
            oldframe->m_topFieldFirst  = (AvFrame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
            oldframe->m_colorspace     = AvFrame->colorspace;
            oldframe->m_colorrange     = AvFrame->color_range;
            oldframe->m_colorprimaries = AvFrame->color_primaries;
            oldframe->m_colortransfer  = AvFrame->color_trc;
            oldframe->m_chromalocation = AvFrame->chroma_location;
            oldframe->m_frameNumber    = m_framesPlayed;
            oldframe->m_frameCounter   = m_frameCounter++;
            oldframe->m_aspect         = m_currentAspect;
            oldframe->m_rotation       = m_videoRotation;
            oldframe->m_stereo3D       = m_stereo3D;

            oldframe->m_dummy               = false;
            oldframe->m_pauseFrame          = false;
            oldframe->m_interlacedReverse   = false;
            oldframe->m_newGOP              = false;
            oldframe->m_deinterlaceInuse    = DEINT_NONE;
            oldframe->m_deinterlaceInuse2x  = false;
            oldframe->m_alreadyDeinterlaced = false;

            m_parent->DiscardVideoFrame(oldframe);
        }
    }

    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "NULL videoframe - direct rendering not "
                                       "correctly initialized.");
        return false;
    }


    if (AvFrame->best_effort_timestamp == AV_NOPTS_VALUE)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No PTS found - unable to process video.");
        return false;
    }
    std::chrono::milliseconds pts = millisecondsFromFloat(av_q2d(Stream->time_base) *
        AvFrame->best_effort_timestamp * 1000);
    std::chrono::milliseconds temppts = pts;
    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!m_ringBuffer->IsDVD() &&
        temppts <= m_lastVPts &&
        (temppts + millisecondsFromFloat(1000 / m_fps) > m_lastVPts ||
         temppts <= 0ms))
    {
        temppts = m_lastVPts;
        temppts += millisecondsFromFloat(1000 / m_fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += millisecondsFromFloat(AvFrame->repeat_pict * 500 / m_fps);
    }

    // Calculate actual fps from the pts values.
    std::chrono::milliseconds ptsdiff = temppts - m_lastVPts;
    double calcfps = 1000.0 / ptsdiff.count();
    if (calcfps < 121.0 && calcfps > 3.0)
    {
        // If fps has doubled due to frame-doubling deinterlace
        // Set fps to double value.
        double fpschange = calcfps / m_fps;
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
            .arg(AvFrame->best_effort_timestamp)
            .arg(pts.count()).arg(temppts.count()).arg(m_lastVPts.count())
            .arg((pts != temppts) ? " fixup" : ""));

    frame->m_interlaced          = (AvFrame->flags & AV_FRAME_FLAG_INTERLACED) != 0;
    frame->m_topFieldFirst       = (AvFrame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) != 0;
    frame->m_newGOP              = m_nextDecodedFrameIsKeyFrame;
    frame->m_repeatPic           = AvFrame->repeat_pict != 0;
    frame->m_displayTimecode     = NormalizeVideoTimecode(Stream, std::chrono::milliseconds(temppts));
    frame->m_frameNumber         = m_framesPlayed;
    frame->m_frameCounter        = m_frameCounter++;
    frame->m_aspect              = m_currentAspect;
    frame->m_colorspace          = AvFrame->colorspace;
    frame->m_colorrange          = AvFrame->color_range;
    frame->m_colorprimaries      = AvFrame->color_primaries;
    frame->m_colortransfer       = AvFrame->color_trc;
    frame->m_chromalocation      = AvFrame->chroma_location;
    frame->m_pixFmt              = AvFrame->format;
    frame->m_deinterlaceInuse    = DEINT_NONE;
    frame->m_rotation            = m_videoRotation;
    frame->m_stereo3D            = m_stereo3D;

    frame->m_dummy               = false;
    frame->m_pauseFrame          = false;
    frame->m_deinterlaceInuse2x  = false;
    frame->m_alreadyDeinterlaced = false;
    frame->m_interlacedReverse   = false;

    // Retrieve HDR metadata
    MythHDRVideoMetadata::Populate(frame, AvFrame);

    m_parent->ReleaseNextVideoFrame(frame, std::chrono::milliseconds(temppts));
    m_mythCodecCtx->PostProcessFrame(context, frame);

    m_nextDecodedFrameIsKeyFrame = false;
    m_decodedVideoFrame = frame;
    m_gotVideoFrame = true;
    if (++m_fpsSkip >= m_fpsMultiplier)
    {
        ++m_framesPlayed;
        m_fpsSkip = 0;
    }

    m_lastVPts = temppts;
    if ((m_firstVPts == 0ms) && m_firstVPtsInuse)
        m_firstVPts = temppts;

    return true;
}

/** \fn AvFormatDecoder::ProcessVBIDataPacket(const AVStream*, const AVPacket*)
 *  \brief Process ivtv proprietary embedded vertical blanking
 *         interval captions.
 *  \sa CC608Decoder, TeletextDecoder
 */
void AvFormatDecoder::ProcessVBIDataPacket(
    [[maybe_unused]] const AVStream *stream, const AVPacket *pkt)
{
    const uint8_t *buf     = pkt->data;
    uint64_t linemask      = 0;
    std::chrono::microseconds utc = m_lastCcPtsu;

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

    static constexpr uint kMinBlank = 6;
    for (uint i = 0; i < 36; i++)
    {
        if (!((linemask >> i) & 0x1))
            continue;

        const uint line  = ((i < 18) ? i : i-18) + kMinBlank;
        const uint field = (i<18) ? 0 : 1;
        const uint id2 = *buf & 0xf;
        switch (id2)
        {
            case V4L2_MPEG_VBI_IVTV_TELETEXT_B:
                // SECAM lines  6-23
                // PAL   lines  6-22
                // NTSC  lines 10-21 (rare)
                m_trackLock.lock();
                if (m_tracks[kTrackTypeTeletextMenu].empty())
                {
                    StreamInfo si {pkt->stream_index, 0};
                    m_trackLock.lock();
                    m_tracks[kTrackTypeTeletextMenu].push_back(si);
                    m_trackLock.unlock();
                    AutoSelectTrack(kTrackTypeTeletextMenu);
                }
                m_trackLock.unlock();
                m_ttd->Decode(buf+1, VBI_IVTV);
                break;
            case V4L2_MPEG_VBI_IVTV_CAPTION_525:
                // PAL   line 22 (rare)
                // NTSC  line 21
                if (21 == line)
                {
                    int data = (buf[2] << 8) | buf[1];
                    if (cc608_good_parity(data))
                        m_ccd608->FormatCCField(duration_cast<std::chrono::milliseconds>(utc), field, data);
                    utc += 33367us;
                }
                break;
            case V4L2_MPEG_VBI_IVTV_VPS: // Video Programming System
                // PAL   line 16
                m_ccd608->DecodeVPS(buf+1); // a.k.a. PDC
                break;
            case V4L2_MPEG_VBI_IVTV_WSS_625: // Wide Screen Signal
                // PAL   line 23
                // NTSC  line 20
                m_ccd608->DecodeWSS(buf+1);
                break;
        }
        buf += 43;
    }
    m_lastCcPtsu = utc;
    UpdateCaptionTracksFromStreams(true, false);
}

/** \fn AvFormatDecoder::ProcessDVBDataPacket(const AVStream*, const AVPacket*)
 *  \brief Process DVB Teletext.
 *  \sa TeletextDecoder
 */
void AvFormatDecoder::ProcessDVBDataPacket(
    const AVStream* /*stream*/, const AVPacket *pkt)
{
    // ETSI EN 301 775 V1.2.1 (2003-05)
    // Check data_identifier value
    // Defined in 4.4.2 Semantics for PES data field, Table 2
    // Support only the "low range" 0x10-0x1F because they have
    // the fixed data_unit_length of 0x2C (44) that the teletext
    // decoder expects.
    //
    const uint8_t *buf     = pkt->data;
    const uint8_t *buf_end = pkt->data + pkt->size;

    if (*buf >= 0x10 && *buf <= 0x1F)
    {
        buf++;
    }
    else
    {
        LOG(VB_VBI, LOG_WARNING, LOC +
            QString("VBI: Unknown data_identier: %1 discarded").arg(*buf));
        return;
    }

    // Process data packets in the PES packet payload
    while (buf < buf_end)
    {
        if (*buf == 0x02)               // data_unit_id 0x02    EBU Teletext non-subtitle data
        {
            buf += 4;                   // Skip data_unit_id, data_unit_length (0x2C, 44) and first two data bytes
            if ((buf_end - buf) >= 42)
                m_ttd->Decode(buf, VBI_DVB);
            buf += 42;
        }
        else if (*buf == 0x03)          // data_unit_id 0x03    EBU Teletext subtitle data
        {
            buf += 4;
            if ((buf_end - buf) >= 42)
                m_ttd->Decode(buf, VBI_DVB_SUBTITLE);
            buf += 42;
        }
        else if (*buf == 0xff)          // data_unit_id 0xff    stuffing
        {
            buf += 46;                  // data_unit_id, data_unit_length and 44 data bytes
        }
        else
        {
            LOG(VB_VBI, LOG_WARNING, LOC +
                QString("VBI: Unsupported data_unit_id: %1 discarded").arg(*buf));
            buf += 46;
        }
    }
}

/** \fn AvFormatDecoder::ProcessDSMCCPacket(const AVStream*, const AVPacket*)
 *  \brief Process DSMCC object carousel packet.
 */
void AvFormatDecoder::ProcessDSMCCPacket([[maybe_unused]] const AVStream *str,
                                         [[maybe_unused]] const AVPacket *pkt)
{
#if CONFIG_MHEG
    if (m_itv == nullptr)
        m_itv = m_parent->GetInteractiveTV();
    if (m_itv == nullptr)
        return;

    // The packet may contain several tables.
    uint8_t *data = pkt->data;
    int length = pkt->size;
    int componentTag = 0;
    int dataBroadcastId = 0;
    unsigned carouselId = 0;
    {
        m_avCodecLock.lock();
        componentTag    = str->component_tag;
        dataBroadcastId = str->data_id;
        carouselId  = (unsigned) str->carousel_id;
        m_avCodecLock.unlock();
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
#endif // CONFIG_MHEG
}

bool AvFormatDecoder::ProcessSubtitlePacket(AVCodecContext* codecContext, AVStream *curstream, AVPacket *pkt)
{
    if (!m_parent->GetSubReader(pkt->stream_index))
        return true;

    long long pts = pkt->pts;
    if (pts == AV_NOPTS_VALUE)
        pts = pkt->dts;
    if (pts == AV_NOPTS_VALUE)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No PTS found - unable to process subtitle.");
        return false;
    }
    pts = static_cast<long long>(av_q2d(curstream->time_base) * pts * 1000);

    m_trackLock.lock();
    int subIdx = m_selectedTrack[kTrackTypeSubtitle].m_av_stream_index;
    int forcedSubIdx = m_selectedForcedTrack[kTrackTypeSubtitle].m_av_stream_index;
    bool mainTrackIsForced = m_selectedTrack[kTrackTypeSubtitle].m_forced;
    bool isForcedTrack = false;
    m_trackLock.unlock();

    int gotSubtitles = 0;
    AVSubtitle subtitle;
    memset(&subtitle, 0, sizeof(AVSubtitle));

    if (m_ringBuffer->IsDVD())
    {
        if (m_ringBuffer->DVD()->NumMenuButtons() > 0)
        {
            m_ringBuffer->DVD()->GetMenuSPUPkt(pkt->data, pkt->size,
                                             curstream->id, pts);
        }
        else
        {
            if (pkt->stream_index == subIdx)
            {
                m_avCodecLock.lock();
                m_ringBuffer->DVD()->DecodeSubtitles(&subtitle, &gotSubtitles,
                                                   pkt->data, pkt->size, pts);
                m_avCodecLock.unlock();
            }
        }
    }
    else if (m_decodeAllSubtitles || pkt->stream_index == subIdx
                                  || pkt->stream_index == forcedSubIdx)
    {
        m_avCodecLock.lock();
        avcodec_decode_subtitle2(codecContext, &subtitle, &gotSubtitles, pkt);
        m_avCodecLock.unlock();

        subtitle.start_display_time += pts;
        subtitle.end_display_time += pts;

        if (pkt->stream_index != subIdx)
            isForcedTrack = true;
    }

    if (gotSubtitles)
    {
        if (isForcedTrack)
        {
            for (unsigned i = 0; i < subtitle.num_rects; i++)
            {
                subtitle.rects[i]->flags |= AV_SUBTITLE_FLAG_FORCED;
            }
        }
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("subtl timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts)
                .arg(subtitle.start_display_time)
                .arg(subtitle.end_display_time));

        bool forcedon = m_parent->GetSubReader(pkt->stream_index)->AddAVSubtitle(
                subtitle, curstream->codecpar->codec_id == AV_CODEC_ID_XSUB,
                isForcedTrack,
                (m_parent->GetAllowForcedSubtitles() && !mainTrackIsForced), false);
         m_parent->EnableForcedSubtitles(forcedon || isForcedTrack);
    }

    return true;
}

bool AvFormatDecoder::ProcessRawTextPacket(AVPacket* Packet)
{
    if (!m_decodeAllSubtitles && m_selectedTrack[kTrackTypeRawText].m_av_stream_index != Packet->stream_index)
        return false;

    auto id = static_cast<uint>(Packet->stream_index + 0x2000);
    if (!m_parent->GetSubReader(id))
        return false;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const auto * codec = QTextCodec::codecForName("utf-8");
    auto text = codec->toUnicode(reinterpret_cast<const char *>(Packet->data), Packet->size - 1);
#else
    auto toUtf16 = QStringDecoder(QStringDecoder::Utf8);
    QString text = toUtf16.decode(Packet->data);
#endif
    auto list = text.split('\n', Qt::SkipEmptyParts);
    m_parent->GetSubReader(id)->AddRawTextSubtitle(list, std::chrono::milliseconds(Packet->duration));
    return true;
}

bool AvFormatDecoder::ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                        [[maybe_unused]] DecodeType decodetype)
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
#if CONFIG_MHEG
            if (!(decodetype & kDecodeVideo))
                m_allowedQuit |= (m_itv && m_itv->ImageHasChanged());
#endif // CONFIG_MHEG:
            break;
        }
        default:
            break;
    }
    return true;
}

int AvFormatDecoder::SetTrack(uint Type, int TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    int ret = DecoderBase::SetTrack(Type, TrackNo);
    if (kTrackTypeAudio == Type)
    {
        QString msg = SetupAudioStream() ? "" : "not ";
        LOG(VB_AUDIO, LOG_INFO, LOC + "Audio stream type " + msg + "changed.");
    }
    return ret;
}

QString AvFormatDecoder::GetTrackDesc(uint type, uint TrackNo)
{
    QMutexLocker locker(&m_trackLock);

    if (!m_ic || TrackNo >= m_tracks[type].size())
        return "";

    bool forced = m_tracks[type][TrackNo].m_forced;
    int lang_key = m_tracks[type][TrackNo].m_language;
    QString forcedString = forced ? QObject::tr(" (forced)") : "";

    int av_index = m_tracks[type][TrackNo].m_av_stream_index;
    AVStream *stream { nullptr };
    if (av_index >= 0 && av_index < (int)m_ic->nb_streams)
        stream = m_ic->streams[av_index];
    AVDictionaryEntry *entry =
        stream ? av_dict_get(stream->metadata, "title", nullptr, 0) : nullptr;
    QString user_title = entry ? QString(R"( "%1")").arg(entry->value) : "";

    if (kTrackTypeAudio == type)
    {
        QString msg = iso639_key_toName(lang_key);

        switch (m_tracks[type][TrackNo].m_audio_type)
        {
            case kAudioTypeNormal :
            {
                if (stream)
                {
                    AVCodecParameters *par = stream->codecpar;
                    AVCodecContext *ctx = m_codecMap.GetCodecContext(stream);
                    if (par->codec_id == AV_CODEC_ID_MP3)
                        msg += QString(" MP3");
                    else if (ctx && ctx->codec)
                        msg += QString(" %1").arg(ctx->codec->name).toUpper();
                    if (!user_title.isEmpty())
                        msg += user_title;

                    int channels = par->ch_layout.nb_channels;

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
                if (!user_title.isEmpty())
                    msg += user_title;
                else
                    msg += QString(" (%1)")
                            .arg(toString(m_tracks[type][TrackNo].m_audio_type));
                break;
        }
        return QString("%1: %2").arg(TrackNo + 1).arg(msg);
    }
    if (kTrackTypeSubtitle == type)
    {
        return QObject::tr("Subtitle") + QString(" %1: %2%3%4")
            .arg(QString::number(TrackNo + 1),
                 iso639_key_toName(lang_key),
                 user_title,
                 forcedString);
    }
    if (forced && kTrackTypeRawText == type)
        return DecoderBase::GetTrackDesc(type, TrackNo) + forcedString;
    return DecoderBase::GetTrackDesc(type, TrackNo);
}

int AvFormatDecoder::GetTeletextDecoderType(void) const
{
    return m_ttd->GetDecoderType();
}

QString AvFormatDecoder::GetXDS(const QString &Key) const
{
    return m_ccd608->GetXDS(Key);
}

QByteArray AvFormatDecoder::GetSubHeader(uint TrackNo)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= m_tracks[kTrackTypeSubtitle].size())
        return {};

    int index = m_tracks[kTrackTypeSubtitle][TrackNo].m_av_stream_index;
    AVCodecContext *ctx = m_codecMap.GetCodecContext(m_ic->streams[index]);
    return ctx ? QByteArray(reinterpret_cast<char*>(ctx->subtitle_header), ctx->subtitle_header_size) :
                 QByteArray();
}

void AvFormatDecoder::GetAttachmentData(uint TrackNo, QByteArray &Filename, QByteArray &Data)
{
    QMutexLocker locker(&m_trackLock);
    if (TrackNo >= m_tracks[kTrackTypeAttachment].size())
        return;

    int index = m_tracks[kTrackTypeAttachment][TrackNo].m_av_stream_index;
    AVDictionaryEntry *tag = av_dict_get(m_ic->streams[index]->metadata, "filename", nullptr, 0);
    if (tag)
        Filename = QByteArray(tag->value);
    AVCodecParameters *par = m_ic->streams[index]->codecpar;
    Data = QByteArray(reinterpret_cast<char*>(par->extradata), par->extradata_size);
}

bool AvFormatDecoder::SetAudioByComponentTag(int Tag)
{
    QMutexLocker locker(&m_trackLock);
    for (size_t i = 0; i < m_tracks[kTrackTypeAudio].size(); i++)
    {
        AVStream *stream = m_ic->streams[m_tracks[kTrackTypeAudio][i].m_av_stream_index];
        if (stream)
            if ((stream->component_tag == Tag) || ((Tag <= 0) && stream->component_tag <= 0))
                return SetTrack(kTrackTypeAudio, static_cast<int>(i)) != -1;
    }
    return false;
}

bool AvFormatDecoder::SetVideoByComponentTag(int Tag)
{
    QMutexLocker locker(&m_trackLock);
    for (uint i = 0; i < m_ic->nb_streams; i++)
    {
        AVStream *stream  = m_ic->streams[i];
        if (stream)
        {
            if (stream->component_tag == Tag)
            {
                StreamInfo si {static_cast<int>(i), 0};
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

    if (m_ringBuffer->IsInDiscMenuOrStillFrame())
        return -1;

    return DecoderBase::AutoSelectTrack(type);
}

static std::vector<int> filter_lang(const sinfo_vec_t &tracks, int lang_key,
                                    const std::vector<int> &ftype)
{
    std::vector<int> ret;

    for (int index : ftype)
    {
        if ((lang_key < 0) || tracks[index].m_language == lang_key)
            ret.push_back(index);
    }

    return ret;
}

static std::vector<int> filter_type(const sinfo_vec_t &tracks, AudioTrackType type)
{
    std::vector<int> ret;

    for (size_t i = 0; i < tracks.size(); i++)
    {
        if (tracks[i].m_audio_type == type)
            ret.push_back(i);
    }

    return ret;
}

int AvFormatDecoder::filter_max_ch(const AVFormatContext *ic,
                                   const sinfo_vec_t     &tracks,
                                   const std::vector<int>&fs,
                                   enum AVCodecID         codecId,
                                   int                    profile)
{
    int selectedTrack = -1;
    int max_seen = -1;

    for (int f : fs)
    {
        const int stream_index = tracks[f].m_av_stream_index;
        AVCodecParameters *par = ic->streams[stream_index]->codecpar;
        if ((codecId == AV_CODEC_ID_NONE || codecId == par->codec_id) &&
            (max_seen < par->ch_layout.nb_channels))
        {
            if (codecId == AV_CODEC_ID_DTS && profile > 0)
            {
                // we cannot decode dts-hd, so only select it if passthrough
                if (!DoPassThrough(par, true) || par->profile != profile)
                    continue;
            }
            selectedTrack = f;
            max_seen = par->ch_layout.nb_channels;
        }
    }

    return selectedTrack;
}

int AvFormatDecoder::selectBestAudioTrack(int lang_key, const std::vector<int> &ftype)
{
    const sinfo_vec_t &atracks = m_tracks[kTrackTypeAudio];
    int selTrack = -1;

    std::vector<int> flang = filter_lang(atracks, lang_key, ftype);

    if (m_audio->CanDTSHD())
    {
        selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                 FF_PROFILE_DTS_HD_MA);
        if (selTrack >= 0)
            return selTrack;
    }
    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_TRUEHD);
    if (selTrack >= 0)
        return selTrack;

    if (m_audio->CanDTSHD())
    {
        selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS,
                                 FF_PROFILE_DTS_HD_HRA);
        if (selTrack >= 0)
            return selTrack;
    }
    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_EAC3);
    if (selTrack >= 0)
        return selTrack;

    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_DTS);
    if (selTrack >= 0)
        return selTrack;

    selTrack = filter_max_ch(m_ic, atracks, flang, AV_CODEC_ID_AC3);
    if (selTrack >= 0)
        return selTrack;

    selTrack = filter_max_ch(m_ic, atracks, flang);

    return selTrack;
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
    QMutexLocker locker(&m_trackLock);

    const sinfo_vec_t &atracks = m_tracks[kTrackTypeAudio];
    StreamInfo        &wtrack  = m_wantedTrack[kTrackTypeAudio];
    StreamInfo        &strack  = m_selectedTrack[kTrackTypeAudio];
    int               &ctrack  = m_currentTrack[kTrackTypeAudio];

    uint numStreams = atracks.size();
    int selTrack = -1;
    if (numStreams > 0)
    {
        if ((ctrack >= 0) && (ctrack < (int)numStreams))
            return ctrack; // audio already selected

        LOG(VB_AUDIO, LOG_DEBUG, QString("%1 available audio streams").arg(numStreams));
        for (const auto & track : atracks)
        {
            AVCodecParameters *codecpar = m_ic->streams[track.m_av_stream_index]->codecpar;
            LOG(VB_AUDIO, LOG_DEBUG, QString("%1: %2 bps, %3 Hz, %4 channels, passthrough(%5)")
                .arg(avcodec_get_name(codecpar->codec_id), QString::number(codecpar->bit_rate),
                QString::number(codecpar->sample_rate), QString::number(codecpar->ch_layout.nb_channels),
                (DoPassThrough(codecpar, true)) ? "true" : "false")
                );
        }

        if (1 == numStreams)
        {
            selTrack = 0;
        }
        else
        {
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

            if ((selTrack < 0) && wlang >= -1)
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

            if (selTrack < 0)
            {
                LOG(VB_AUDIO, LOG_INFO, LOC + "Trying to select audio track (w/lang)");

                // Filter out commentary and audio description tracks
                std::vector<int> ftype = filter_type(atracks, kAudioTypeNormal);

                if (ftype.empty())
                {
                    LOG(VB_AUDIO, LOG_WARNING, "No audio tracks matched the type filter, "
                                               "so trying all tracks.");
                    for (int i = 0; i < static_cast<int>(atracks.size()); i++)
                        ftype.push_back(i);
                }

                // Try to get the language track for the preferred language for audio
                QString language_key_convert = iso639_str2_to_str3(gCoreContext->GetAudioLanguage());
                uint language_key = iso639_str3_to_key(language_key_convert);
                uint canonical_key = iso639_key_to_canonical_key(language_key);

                selTrack = selectBestAudioTrack(canonical_key, ftype);

                // Try to get best track for most preferred language for audio.
                // Set by the "Guide Data" "Audio Language" preference in Appearance.
                if (selTrack < 0)
                {
                    auto it = m_languagePreference.begin();
                    for (; it !=  m_languagePreference.end() && selTrack < 0; ++it)
                    {
                        selTrack = selectBestAudioTrack(*it, ftype);
                    }
                }

                // Could not select track based on user preferences (audio language)
                // Try to select the default track
                if (selTrack < 0)
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

                // Try to get best track for any language
                if (selTrack < 0)
                {
                    LOG(VB_AUDIO, LOG_INFO, LOC +
                        "Trying to select audio track (wo/lang)");
                    selTrack = selectBestAudioTrack(-1, ftype);
                }
            }
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

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Selected track %1 (A/V Stream #%2)")
                .arg(static_cast<uint>(ctrack)).arg(strack.m_av_stream_index));
    }

    SetupAudioStream();
    return selTrack;
}

static void extract_mono_channel(uint channel, AudioInfo *audioInfo,
                                 char *buffer, int bufsize)
{
    // Only stereo -> mono (left or right) is supported
    if (audioInfo->m_channels != 2)
        return;

    if (channel >= (uint)audioInfo->m_channels)
        return;

    const uint samplesize = audioInfo->m_sampleSize;
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

bool AvFormatDecoder::ProcessAudioPacket(AVCodecContext* context, AVStream *curstream, AVPacket *pkt,
                                         DecodeType decodetype)
{
    int ret             = 0;
    int data_size       = 0;
    bool firstloop      = true;
    int decoded_size    = -1;

    m_trackLock.lock();
    int audIdx = m_selectedTrack[kTrackTypeAudio].m_av_stream_index;
    int audSubIdx = m_selectedTrack[kTrackTypeAudio].m_av_substream_index;
    m_trackLock.unlock();

    AVPacket *tmp_pkt = av_packet_alloc();
    tmp_pkt->data = pkt->data;
    tmp_pkt->size = pkt->size;
    while (tmp_pkt->size > 0)
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
        bool isDual = is_dual_mono(context->ch_layout);
        if ((wasDual && !isDual) || (!wasDual &&  isDual))
        {
            SetupAudioStreamSubIndexes(audIdx);
            reselectAudioTrack = true;
        }

        // detect channels on streams that need
        // to be decoded before we can know this
        bool already_decoded = false;
        if (!context->ch_layout.nb_channels)
        {
            m_avCodecLock.lock();
            if (DoPassThrough(curstream->codecpar, false) || !DecoderWillDownmix(context))
            {
                // for passthru or codecs for which the decoder won't downmix
                // let the decoder set the number of channels. For other codecs
                // we downmix if necessary in audiooutputbase
                ;
            }
            else // No passthru, the decoder will downmix
            {
                AVChannelLayout channel_layout;
                av_channel_layout_default(&channel_layout, m_audio->GetMaxChannels());
                av_opt_set_chlayout(context->priv_data, "downmix", &channel_layout, 0);

                if (context->codec_id == AV_CODEC_ID_AC3)
                    context->ch_layout.nb_channels = m_audio->GetMaxChannels();
            }

            ret = m_audio->DecodeAudio(context, m_audioSamples, data_size, tmp_pkt);
            decoded_size = data_size;
            already_decoded = true;
            reselectAudioTrack |= context->ch_layout.nb_channels;
            m_avCodecLock.unlock();
        }

        if (reselectAudioTrack)
        {
            QMutexLocker locker(&m_trackLock);
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
            m_lastAPts = millisecondsFromFloat(av_q2d(curstream->time_base) * pkt->pts * 1000);

        m_firstVPtsInuse = false;
        m_avCodecLock.lock();
        data_size = 0;

        // Check if the number of channels or sampling rate have changed
        if (context->sample_rate != m_audioOut.m_sampleRate ||
            context->ch_layout.nb_channels    != m_audioOut.m_channels ||
            AudioOutputSettings::AVSampleFormatToFormat(context->sample_fmt,
                                                        context->bits_per_raw_sample) != m_audioOut.format)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Audio stream changed");
            if (context->ch_layout.nb_channels != m_audioOut.m_channels)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Number of audio channels changed from %1 to %2")
                    .arg(m_audioOut.m_channels).arg(context->ch_layout.nb_channels));
            }
            QMutexLocker locker(&m_trackLock);
            m_currentTrack[kTrackTypeAudio] = -1;
            m_selectedTrack[kTrackTypeAudio].m_av_stream_index = -1;
            audIdx = -1;
            AutoSelectAudioTrack();
        }

        if (m_audioOut.m_doPassthru)
        {
            if (!already_decoded)
            {
                if (m_audio->NeedDecodingBeforePassthrough())
                {
                    ret = m_audio->DecodeAudio(context, m_audioSamples, data_size, tmp_pkt);
                    decoded_size = data_size;
                }
                else
                {
                    decoded_size = -1;
                }
            }
            memcpy(m_audioSamples, tmp_pkt->data, tmp_pkt->size);
            data_size = tmp_pkt->size;
            // We have processed all the data, there can't be any left
            tmp_pkt->size = 0;
        }
        else
        {
            if (!already_decoded)
            {
                if (DecoderWillDownmix(context))
                {
                    AVChannelLayout channel_layout;
                    av_channel_layout_default(&channel_layout, m_audio->GetMaxChannels());
                    av_opt_set_chlayout(context->priv_data, "downmix", &channel_layout, 0);
                }

                ret = m_audio->DecodeAudio(context, m_audioSamples, data_size, tmp_pkt);
                decoded_size = data_size;
            }
        }
        m_avCodecLock.unlock();

        if (ret < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown audio decoding error");
            av_packet_free(&tmp_pkt);
            return false;
        }

        if (data_size <= 0)
        {
            tmp_pkt->data += ret;
            tmp_pkt->size -= ret;
            continue;
        }

        std::chrono::milliseconds temppts = m_lastAPts;

        if (audSubIdx != -1 && !m_audioOut.m_doPassthru)
            extract_mono_channel(audSubIdx, &m_audioOut,
                                 (char *)m_audioSamples, data_size);

        int samplesize = AudioOutputSettings::SampleSize(m_audio->GetFormat());
        int frames = (context->ch_layout.nb_channels <= 0 || decoded_size < 0 || !samplesize) ? -1 :
            decoded_size / (context->ch_layout.nb_channels * samplesize);
        m_audio->AddAudioData((char *)m_audioSamples, data_size, temppts, frames);
        if (m_audioOut.m_doPassthru && !m_audio->NeedDecodingBeforePassthrough())
        {
            m_lastAPts += m_audio->LengthLastData();
        }
        else
        {
            m_lastAPts += millisecondsFromFloat
                ((double)(frames * 1000) / context->sample_rate);
        }

        LOG(VB_TIMESTAMP, LOG_INFO, LOC + QString("audio timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts).arg(temppts.count()).arg(m_lastAPts.count()));

        m_allowedQuit |= m_ringBuffer->IsInStillFrame() ||
                       m_audio->IsBufferAlmostFull();

        tmp_pkt->data += ret;
        tmp_pkt->size -= ret;
        firstloop = false;
    }

    av_packet_free(&tmp_pkt);
    return true;
}

// documented in decoderbase.h
bool AvFormatDecoder::GetFrame(DecodeType decodetype, bool &Retry)
{
    AVPacket *pkt = nullptr;
    bool have_err = false;

    const DecodeType origDecodetype = decodetype;

    m_gotVideoFrame = false;

    m_frameDecoded = 0;
    m_decodedVideoFrame = nullptr;

    m_allowedQuit = false;
    bool storevideoframes = false;

    m_skipAudio = (m_lastVPts == 0ms);

    if( !m_processFrames )
    {
        return false;
    }

    m_hasVideo = HasVideo();
    m_needDummyVideoFrames = false;

    if (!m_hasVideo && (decodetype & kDecodeVideo))
    {
        // NB This could be an issue if the video stream is not
        // detected initially as the video buffers will be filled.
        m_needDummyVideoFrames = true;
        decodetype = (DecodeType)((int)decodetype & ~kDecodeVideo);
        m_skipAudio = false;
    }

    m_allowedQuit = m_audio->IsBufferAlmostFull();

    while (!m_allowedQuit)
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
                    m_allowedQuit = true;
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
                m_allowedQuit = true;
            }
            else if ((decodetype & kDecodeAV) == kDecodeAV &&
                     (m_storedPackets.count() < kMaxVideoQueueSize) &&
                     // buffer audio to prevent audio buffer
                     // underruns in case you are setting negative values
                     // in Adjust Audio Sync.
                     m_lastAPts < m_lastVPts + m_audioReadAhead &&
                     !m_ringBuffer->IsInStillFrame())
            {
                storevideoframes = true;
            }
            else if (decodetype & kDecodeVideo)
            {
                if (m_storedPackets.count() >= kMaxVideoQueueSize)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Audio %1 ms behind video but already %2 "
                                "video frames queued. AV-Sync might be broken.")
                            .arg((m_lastVPts-m_lastAPts).count()).arg(m_storedPackets.count()));
                }
                m_allowedQuit = true;
                continue;
            }
        }

        if (!storevideoframes && m_storedPackets.count() > 0)
        {
            if (pkt)
                av_packet_free(&pkt);
            pkt = m_storedPackets.takeFirst();
        }
        else
        {
            if (!pkt)
                pkt = av_packet_alloc();

            int retval = 0;
            if (m_ic != nullptr)
                retval = ReadPacket(m_ic, pkt, storevideoframes);
            if ((m_ic == nullptr) || (retval < 0))
            {
                if (retval == -EAGAIN)
                    continue;

                SetEof(true);
                av_packet_free(&pkt);

                LOG(VB_GENERAL, LOG_ERR, QString("decoding error %1 (%2)")
                    .arg(QString::fromStdString(av_make_error_stdstring_unknown(retval)),
                        QString::number(retval)));
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
        const AVCodecID codec_id = curstream->codecpar->codec_id;

        // Handle AVCodecID values that don't have an AVCodec decoder and
        // store video packets
        switch (codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            if (storevideoframes)
            {
                m_storedPackets.append(pkt);
                pkt = nullptr;
                continue;
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            // FFmpeg does not currently have an AC-4 decoder
            if (codec_id == AV_CODEC_ID_AC4)
            {
                av_packet_unref(pkt);
                continue;
            }
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            switch (codec_id)
            {
            case AV_CODEC_ID_TEXT:
                ProcessRawTextPacket(pkt);
                av_packet_unref(pkt);
                continue;
            case AV_CODEC_ID_DVB_TELETEXT:
                ProcessDVBDataPacket(curstream, pkt);
                av_packet_unref(pkt);
                continue;
            default:
                break;
            }
            break;
        case AVMEDIA_TYPE_DATA:
            ProcessDataPacket(curstream, pkt, decodetype);
            av_packet_unref(pkt);
            continue;
        default:
            break;
        }

        // ensure there is an AVCodecContext for this stream
        AVCodecContext *context = m_codecMap.GetCodecContext(curstream);
        if (context == nullptr)
        {
            if (codec_type != AVMEDIA_TYPE_VIDEO)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("No codec for stream index %1, type(%2) id(%3:%4)")
                        .arg(QString::number(pkt->stream_index),
                             AVMediaTypeToString(codec_type),
                             avcodec_get_name(codec_id),
                             QString::number(codec_id)
                            )
                   );
                // Process Stream Change in case we have no audio
                if (codec_type == AVMEDIA_TYPE_AUDIO && !m_audio->HasAudioIn())
                    m_streamsChanged = true;
            }
            av_packet_unref(pkt);
            continue;
        }

        have_err = false;

        switch (codec_type)
        {
            case AVMEDIA_TYPE_AUDIO:
            {
                if (!ProcessAudioPacket(context, curstream, pkt, decodetype))
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

                if (!PreProcessVideoPacket(context, curstream, pkt))
                    continue;

                // If the resolution changed in XXXPreProcessPkt, we may
                // have a fatal error, so check for this before continuing.
                if (m_parent->IsErrored())
                {
                    av_packet_free(&pkt);
                    return false;
                }

                if (pkt->pts != AV_NOPTS_VALUE)
                {
                    m_lastCcPtsu = microsecondsFromFloat
                                 (av_q2d(curstream->time_base)*pkt->pts*1000000);
                }

                if (!(decodetype & kDecodeVideo))
                {
                    m_framesPlayed++;
                    m_gotVideoFrame = true;
                    break;
                }

                if (!ProcessVideoPacket(context, curstream, pkt, Retry))
                    have_err = true;
                break;
            }

            case AVMEDIA_TYPE_SUBTITLE:
            {
                if (!ProcessSubtitlePacket(context, curstream, pkt))
                    have_err = true;
                break;
            }

            default:
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("Decoding - id(%1) type(%2)")
                        .arg(avcodec_get_name(codec_id),
                             AVMediaTypeToString(codec_type)));
                have_err = true;
                break;
            }
        }

        if (!have_err && !Retry)
            m_frameDecoded = 1;
        av_packet_unref(pkt);
        if (Retry)
            break;
    }

    av_packet_free(&pkt);
    return true;
}

void AvFormatDecoder::StreamChangeCheck(void)
{
    if (m_streamsChanged)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StreamChangeCheck skip SeekReset"));
        // SeekReset(0, 0, true, true);
        ScanStreams(false);
        m_streamsChanged = false;
    }
}

int AvFormatDecoder::ReadPacket(AVFormatContext *ctx, AVPacket *pkt, bool &/*storePacket*/)
{
    m_avCodecLock.lock();
    int result = av_read_frame(ctx, pkt);
    m_avCodecLock.unlock();
    return result;
}

bool AvFormatDecoder::HasVideo()
{
    MythAVBufferRef pmt_buffer {get_pmt_section_from_AVProgram(get_current_AVProgram())};
    if (pmt_buffer.has_buffer())
    {
        const ProgramMapTable pmt(PSIPTable(pmt_buffer.data()));

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
    while (m_needDummyVideoFrames && m_parent && m_parent->GetFreeVideoFrames())
    {
        MythVideoFrame *frame = m_parent->GetNextVideoFrame();
        if (!frame)
            return false;

        frame->ClearMetadata();
        frame->ClearBufferToBlank();

        frame->m_dummy        = true;
        frame->m_frameNumber  = m_framesPlayed;
        frame->m_frameCounter = m_frameCounter++;

        m_parent->ReleaseNextVideoFrame(frame, m_lastVPts);
        m_parent->DeLimboFrame(frame);

        m_decodedVideoFrame = frame;
        m_framesPlayed++;
        m_gotVideoFrame = true;
    }
    return true;
}

QString AvFormatDecoder::GetCodecDecoderName(void) const
{
    return get_decoder_name(m_videoCodecId);
}

QString AvFormatDecoder::GetRawEncodingType(void)
{
    int stream = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    if (stream < 0 || !m_ic)
        return {};
    return avcodec_get_name(m_ic->streams[stream]->codecpar->codec_id);
}

void AvFormatDecoder::SetDisablePassThrough(bool disable)
{
    if (m_selectedTrack[kTrackTypeAudio].m_av_stream_index < 0)
    {
        m_disablePassthru = disable;
        return;
    }

    if (disable != m_disablePassthru)
    {
        m_disablePassthru = disable;
        QString msg = (disable) ? "Disabling" : "Allowing";
        LOG(VB_AUDIO, LOG_INFO, LOC + msg + " pass through");

        // Force pass through state to be reanalyzed
        ForceSetupAudioStream();
    }
}

void AvFormatDecoder::ForceSetupAudioStream(void)
{
    QMutexLocker locker(&m_trackLock);
    SetupAudioStream();
}

inline bool AvFormatDecoder::DecoderWillDownmix(const AVCodecContext *ctx)
{
    // Until ffmpeg properly implements dialnorm
    // use Myth internal downmixer if machine has SSE2
    if (m_audio->CanDownmix() && AudioOutputUtil::has_optimized_SIMD())
        return false;
    // use ffmpeg only for dolby codecs if we have to
    //return av_opt_find(ctx->priv_data, "downmix", nullptr, 0, 0);
    // av_opt_find was causing segmentation faults, so explicitly list the
    // compatible decoders
    switch (ctx->codec_id)
    {
        case AV_CODEC_ID_AC3:
        case AV_CODEC_ID_TRUEHD:
        case AV_CODEC_ID_EAC3:
        case AV_CODEC_ID_MLP:
        case AV_CODEC_ID_DTS:
            return true;
        default:
           return false;
    }
}

bool AvFormatDecoder::DoPassThrough(const AVCodecParameters *par, bool withProfile)
{
    bool passthru = false;

    // if withProfile == false, we will accept any DTS stream regardless
    // of its profile. We do so, so we can bitstream DTS-HD as DTS core
    if (!withProfile && par->codec_id == AV_CODEC_ID_DTS && !m_audio->CanDTSHD())
    {
        passthru = m_audio->CanPassthrough(par->sample_rate, par->ch_layout.nb_channels,
                                           par->codec_id, FF_PROFILE_DTS);
    }
    else
    {
        passthru = m_audio->CanPassthrough(par->sample_rate, par->ch_layout.nb_channels,
                                           par->codec_id, par->profile);
    }

    passthru &= !m_disablePassthru;

    return passthru;
}

/** \fn AvFormatDecoder::SetupAudioStream(void)
 *  \brief Reinitializes audio if it needs to be reinitialized.
 *
 *  \return true if audio changed, false otherwise
 */
bool AvFormatDecoder::SetupAudioStream(void)
{
    AudioInfo info; // no_audio
    AVStream *curstream = nullptr;
    AVCodecContext *ctx = nullptr;
    AudioInfo old_in    = m_audioIn;
    int requested_channels = 0;

    if ((m_currentTrack[kTrackTypeAudio] >= 0) && m_ic &&
        (m_selectedTrack[kTrackTypeAudio].m_av_stream_index <=
         (int) m_ic->nb_streams))
    {
        curstream = m_ic->streams[m_selectedTrack[kTrackTypeAudio]
                                 .m_av_stream_index];
        if (curstream != nullptr)
            ctx = m_codecMap.GetCodecContext(curstream);
    }

    if (ctx == nullptr)
    {
        if (!m_tracks[kTrackTypeAudio].empty())
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "No codec context. Returning false");
        return false;
    }

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

    requested_channels = ctx->ch_layout.nb_channels;

    if (!using_passthru &&
        ctx->ch_layout.nb_channels > (int)m_audio->GetMaxChannels() &&
        DecoderWillDownmix(ctx))
    {
        requested_channels = m_audio->GetMaxChannels();

        AVChannelLayout channel_layout;
        av_channel_layout_default(&channel_layout, requested_channels);
        av_opt_set_chlayout(ctx->priv_data, "downmix", &channel_layout, 0);
    }

    info = AudioInfo(ctx->codec_id, fmt, ctx->sample_rate,
                     requested_channels, using_passthru, ctx->ch_layout.nb_channels,
                     ctx->codec_id == AV_CODEC_ID_DTS ? ctx->profile : 0);
    if (info == m_audioIn)
        return false;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Initializing audio parms from " +
        QString("audio track #%1").arg(m_currentTrack[kTrackTypeAudio]+1));

    m_audioOut = m_audioIn = info;

    LOG(VB_AUDIO, LOG_INFO, LOC + "Audio format changed " +
        QString("\n\t\t\tfrom %1 to %2")
            .arg(old_in.toString(), m_audioOut.toString()));

    m_audio->SetAudioParams(m_audioOut.format, ctx->ch_layout.nb_channels,
                            requested_channels,
                            m_audioOut.m_codecId, m_audioOut.m_sampleRate,
                            m_audioOut.m_doPassthru, m_audioOut.m_codecProfile);
    m_audio->ReinitAudio();
    AudioOutput *audioOutput = m_audio->GetAudioOutput();
    if (audioOutput)
        audioOutput->SetSourceBitrate(ctx->bit_rate);

    if (LCD *lcd = LCD::Get())
    {
        LCDAudioFormatSet audio_format = AUDIO_MP3;

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

        if (m_audioOut.m_doPassthru)
            lcd->setVariousLEDs(VARIOUS_SPDIF, true);
        else
            lcd->setVariousLEDs(VARIOUS_SPDIF, false);

        switch (m_audioIn.m_channels)
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
    int64_t start_time = INT64_MAX;
    int64_t end_time = INT64_MIN;
    AVStream *st = nullptr;

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

   int64_t duration = INT64_MIN;
   if (st->start_time != AV_NOPTS_VALUE && st->time_base.den) {
       int64_t start_time1= av_rescale_q(st->start_time, st->time_base, AV_TIME_BASE_Q);
       start_time = std::min(start_time1, start_time);
       if (st->duration != AV_NOPTS_VALUE) {
           int64_t end_time1 = start_time1 +
                      av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
           end_time = std::max(end_time1, end_time);
       }
   }
   if (st->duration != AV_NOPTS_VALUE) {
       int64_t duration1 = av_rescale_q(st->duration, st->time_base, AV_TIME_BASE_Q);
       duration = std::max(duration1, duration);
   }
    if (start_time != INT64_MAX) {
        ic->start_time = start_time;
        if (end_time != INT64_MIN) {
            duration = std::max(end_time - start_time, duration);
        }
    }
    if (duration != INT64_MIN) {
        ic->duration = duration;
        if (!ic->pb)
            return;
        int64_t filesize = avio_size(ic->pb);
        if (filesize > 0) {
            /* compute the bitrate */
            ic->bit_rate = (double)filesize * 8.0 * AV_TIME_BASE /
                (double)ic->duration;
        }
    }
}

int AvFormatDecoder::get_current_AVStream_index(TrackType type)
{
    if (m_currentTrack[type] < 0 || static_cast<size_t>(m_currentTrack[type]) >= m_tracks[type].size())
    {
        return -1;
    }
    return m_tracks[type][m_currentTrack[type]].m_av_stream_index;
}

AVProgram* AvFormatDecoder::get_current_AVProgram()
{
    if (m_ic == nullptr)
    {
        return nullptr;
    }
    AVProgram* program = av_find_program_from_stream(m_ic, nullptr, get_current_AVStream_index(kTrackTypeAudio));
    if (program == nullptr)
    {
        program = av_find_program_from_stream(m_ic, nullptr, get_current_AVStream_index(kTrackTypeVideo));
    }
    return program;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
