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

#ifdef USING_MEDIACODEC
extern "C" {
#include "libavcodec/jni.h"
}
#include <QtAndroidExtras>
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
#include "libavutil/display.h"
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

// Maximum number of sequential invalid data packet errors before we try
// switching to software decoder. Packet errors are often seen when using
// hardware contexts and, for example, seeking. Hence this needs to be high and
// is probably best removed as it is treating the symptoms and not the cause.
// See also comment in MythCodecMap::freeCodecContext re trying to free an
// active hardware context when it is errored.
#define SEQ_PKT_ERR_MAX 50

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
#ifdef USING_DXVA2
int  get_avf_buffer_dxva2(struct AVCodecContext *c, AVFrame *pic, int flags);
#endif

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

static bool StreamHasRequiredParameters(AVStream *Stream)
{
    AVCodecContext *avctx = nullptr;
    switch (Stream->codecpar->codec_type)
    {
        // We fail on video first as this is generally the most serious error
        // and if we have video, we usually have everything else
        case AVMEDIA_TYPE_VIDEO:
            avctx = gCodecMap->getCodecContext(Stream);
            if (!avctx)
                FAIL("No codec for video stream");
            if (!Stream->codecpar->width || !Stream->codecpar->height)
                FAIL("Unspecified video size");
            if (Stream->codecpar->format == AV_PIX_FMT_NONE)
                FAIL("Unspecified video pixel format");
            if (avctx->codec_id == AV_CODEC_ID_RV30 || avctx->codec_id == AV_CODEC_ID_RV40)
                if (!Stream->sample_aspect_ratio.num && !avctx->sample_aspect_ratio.num && !Stream->codec_info_nb_frames)
                    FAIL("No frame in rv30/40 and no sar");
            break;
        case AVMEDIA_TYPE_AUDIO:
            avctx = gCodecMap->getCodecContext(Stream);
            if (!avctx)
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
            if (!Stream->nb_decoded_frames && avctx->codec_id == AV_CODEC_ID_DTS)
                FAIL("No decodable DTS frames");
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
    if (silence_ffmpeg_logging)
        return;

    if (VERBOSE_LEVEL_NONE)
        return;

    static QString s_fullLine("");
    static constexpr int kMsgLen = 255;
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
        case AV_LOG_TRACE:
            verbose_level = LOG_DEBUG;
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

    char str[kMsgLen+1];
    int bytes = vsnprintf(str, kMsgLen+1, fmt, vl);

    // check for truncated messages and fix them
    if (bytes > kMsgLen)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("Libav log output truncated %1 of %2 bytes written")
                .arg(kMsgLen).arg(bytes));
        str[kMsgLen-1] = '\n';
    }

    s_fullLine += QString(str);
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

void AvFormatDecoder::GetDecoders(RenderOptions &opts)
{
    opts.decoders->append("ffmpeg");
    (*opts.equiv_decoders)["ffmpeg"].append("nuppel");
    (*opts.equiv_decoders)["ffmpeg"].append("dummy");

    MythCodecContext::GetDecoders(opts);
    PrivateDecoder::GetDecoders(opts);
}

AvFormatDecoder::AvFormatDecoder(MythPlayer *parent,
                                 const ProgramInfo &pginfo,
                                 PlayerFlags flags)
    : DecoderBase(parent, pginfo),
      m_isDbIgnored(gCoreContext->IsDatabaseIgnored()),
      m_h264Parser(new H264Parser()),
      m_playerFlags(flags),
      // Closed Caption & Teletext decoders
      m_ccd608(new CC608Decoder(parent->GetCC608Reader())),
      m_ccd708(new CC708Decoder(parent->GetCC708Reader())),
      m_ttd(new TeletextDecoder(parent->GetTeletextReader()))
{
    // this will be deleted and recreated once decoder is set up
    m_mythCodecCtx = new MythCodecContext(this, kCodec_NONE);

    m_audioSamples = (uint8_t *)av_mallocz(AudioOutput::kMaxSizeBuffer);
    m_ccd608->SetIgnoreTimecode(true);

    av_log_set_callback(myth_av_log);

    m_audioIn.m_sampleSize = -32;// force SetupAudioStream to run once
    m_itv = m_parent->GetInteractiveTV();

    cc608_build_parity_table(m_cc608ParityTable);

    AvFormatDecoder::SetIdrOnlyKeyframes(true);
    m_audioReadAhead = gCoreContext->GetNumSetting("AudioReadAhead", 100);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("PlayerFlags: 0x%1, AudioReadAhead: %2 msec")
        .arg(m_playerFlags, 0, 16).arg(m_audioReadAhead));
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
    delete m_privateDec;
    delete m_h264Parser;
    delete m_mythCodecCtx;

    sws_freeContext(m_swsCtx);

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

    delete m_privateDec;
    m_privateDec = nullptr;
    m_h264Parser->Reset();
}

static int64_t lsb3full(int64_t lsb, int64_t base_ts, int lsb_bits)
{
    int64_t mask = (lsb_bits < 64) ? (1LL<<lsb_bits)-1 : -1LL;
    return  ((lsb - base_ts)&mask);
}

int64_t AvFormatDecoder::NormalizeVideoTimecode(int64_t timecode)
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
        return 0;

    if (m_ic->start_time != AV_NOPTS_VALUE)
    {
        start_pts = av_rescale(m_ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);
    }

    int64_t pts = av_rescale(timecode / 1000.0,
                     st->time_base.den,
                     st->time_base.num);

    // adjust for start time and wrap
    pts = lsb3full(pts, start_pts, st->pts_wrap_bits);

    return (int64_t)(av_q2d(st->time_base) * pts * 1000);
}

int64_t AvFormatDecoder::NormalizeVideoTimecode(AVStream *st,
                                                int64_t timecode)
{
    int64_t start_pts = 0;

    if (m_ic->start_time != AV_NOPTS_VALUE)
    {
        start_pts = av_rescale(m_ic->start_time,
                               st->time_base.den,
                               AV_TIME_BASE * (int64_t)st->time_base.num);
    }

    int64_t pts = av_rescale(timecode / 1000.0,
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

    m_doRewind = true;

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

    bool oldrawstate = m_getRawFrames;
    m_getRawFrames = false;

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
    if (seekDelta >= 0 && seekDelta < 2 && !m_doRewind && m_parent->GetPlaySpeed() == 0.0F)
    {
        SeekReset(m_framesPlayed, seekDelta, false, true);
        m_parent->SetFramesPlayed(m_framesPlayed + 1);
        m_getRawFrames = oldrawstate;
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

    int flags = (m_doRewind || exactseeks) ? AVSEEK_FLAG_BACKWARD : 0;

    if (av_seek_frame(m_ic, -1, ts, flags) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_seek_frame(ic, -1, %1, 0) -- error").arg(ts));
        m_getRawFrames = oldrawstate;
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
        m_noDtsHack = false;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No DTS Seeking Hack!");
        m_noDtsHack = true;
        m_framesPlayed = desiredFrame;
        m_fpsSkip = 0;
        m_framesRead = desiredFrame;
        normalframes = 0;
    }

    SeekReset(m_lastKey, normalframes, true, discardFrames);

    if (discardFrames)
        m_parent->SetFramesPlayed(m_framesPlayed + 1);

    m_doRewind = false;

    m_getRawFrames = oldrawstate;

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
            .arg((doflush) ? "do" : "don't")
            .arg((discardFrames) ? "do" : "don't"));

    DecoderBase::SeekReset(newKey, skipFrames, doflush, discardFrames);

    QMutexLocker locker(avcodeclock);

    // Discard all the queued up decoded frames
    if (discardFrames)
    {
        bool releaseall = m_mythCodecCtx ? (m_mythCodecCtx->DecoderWillResetOnFlush() ||
                                            m_mythCodecCtx->DecoderNeedsReset(nullptr)) : false;
        m_parent->DiscardVideoFrames(doflush, doflush && releaseall);
    }

    if (doflush)
    {
        m_lastAPts = 0;
        m_lastVPts = 0;
        m_lastCcPtsu = 0;
        m_faultyPts = m_faultyDts = 0;
        m_lastPtsForFaultDetection = 0;
        m_lastDtsForFaultDetection = 0;
        m_ptsDetected = false;
        m_reorderedPtsDetected = false;

        ff_read_frame_flush(m_ic);

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
            AVCodecContext *enc = gCodecMap->hasCodecContext(m_ic->streams[i]);
            // note that contexts that have not been opened have
            // enc->internal = nullptr and cause a segfault in
            // avcodec_flush_buffers
            if (enc && enc->internal)
                avcodec_flush_buffers(enc);
        }
        if (m_privateDec)
            m_privateDec->Reset();

        // Free up the stored up packets
        while (!m_storedPackets.isEmpty())
        {
            AVPacket *pkt = m_storedPackets.takeFirst();
            av_packet_unref(pkt);
            delete pkt;
        }

        m_prevGopPos = 0;
        m_gopSet = false;
        if (!m_ringBuffer->IsDVD())
        {
            if (!m_noDtsHack)
            {
                m_framesPlayed = m_lastKey;
                m_fpsSkip = 0;
                m_framesRead = m_lastKey;
            }

            m_noDtsHack = false;
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
                usleep(1000);
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
        m_firstVPts = 0;
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
    int buf_size                  = m_ringBuffer->BestBufferSize();
    int streamed                  = m_ringBuffer->IsStreamed();
    m_readContext.prot            = AVFRingBuffer::GetRingBufferURLProtocol();
    m_readContext.flags           = AVIO_FLAG_READ;
    m_readContext.is_streamed     = streamed;
    m_readContext.max_packet_size = 0;
    m_readContext.priv_data       = m_avfRingBuffer;
    auto *buffer                  = (unsigned char *)av_malloc(buf_size);
    m_ic->pb                      = avio_alloc_context(buffer, buf_size, 0,
                                                      &m_readContext,
                                                      AVFRingBuffer::AVF_Read_Packet,
                                                      AVFRingBuffer::AVF_Write_Packet,
                                                      AVFRingBuffer::AVF_Seek_Packet);

    // We can always seek during LiveTV
    m_ic->pb->seekable            = !streamed || forceseek;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Buffer size: %1 Streamed %2 Seekable %3 Available %4")
        .arg(buf_size).arg(streamed).arg(m_ic->pb->seekable).arg(m_ringBuffer->GetReadBufAvail()));
}

extern "C" void HandleStreamChange(void *data)
{
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(data);

    int cnt = decoder->m_ic->nb_streams;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("streams_changed 0x%1 -- stream count %2")
            .arg((uint64_t)data,0,16).arg(cnt));

    decoder->m_streamsChanged = true;
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

    m_ringBuffer = rbuffer;

    // Process frames immediately unless we're decoding
    // a DVD, in which case don't so that we don't show
    // anything whilst probing the data streams.
    m_processFrames = !m_ringBuffer->IsDVD();

    delete m_avfRingBuffer;
    m_avfRingBuffer = new AVFRingBuffer(rbuffer);

    AVInputFormat *fmt      = nullptr;
    QString        fnames   = m_ringBuffer->GetFilename();
    QByteArray     fnamea   = fnames.toLatin1();
    const char    *filename = fnamea.constData();

    AVProbeData probe;
    memset(&probe, 0, sizeof(AVProbeData));
    probe.filename = filename;
    probe.buf = reinterpret_cast<unsigned char *>(testbuf);
    if (testbufsize + AVPROBE_PADDING_SIZE <= kDecoderProbeBufferSize)
        probe.buf_size = testbufsize;
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
        AVInputFormat *fmt2 = av_find_input_format("mpegts-ffmpeg");
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

            m_avfRingBuffer->SetInInit(m_livetv);
            InitByteContext(m_livetv);

            err = avformat_open_input(&m_ic, filename, fmt, nullptr);
            if (err < 0)
            {
                char error[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(error, sizeof(error), err);
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Failed to open input ('%1')").arg(error));

                // note - m_ic (AVFormatContext) is freed on failure
                if (retries > 1)
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
        m_ic->max_analyze_duration = 60 * AV_TIME_BASE;

        m_avfRingBuffer->SetInInit(m_livetv);
        err = FindStreamInfo();
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
            if (!StreamHasRequiredParameters(m_ic->streams[i]))
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

    fmt->flags &= ~AVFMT_NOFILE;

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

    AutoSelectTracks(); // This is needed for transcoder

#ifdef USING_MHEG
    {
        int initialAudio = -1;
        int initialVideo = -1;
        if (m_itv || (m_itv = m_parent->GetInteractiveTV()))
            m_itv->GetInitialStreams(initialAudio, initialVideo);
        if (initialAudio >= 0)
            SetAudioByComponentTag(initialAudio);
        if (initialVideo >= 0)
            SetVideoByComponentTag(initialVideo);
    }
#endif // USING_MHEG

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
    int64_t dur = 0;

    if (m_playbackInfo)
    {
        dur = m_playbackInfo->QueryTotalDuration();
        dur /= 1000;
    }

    if (dur == 0)
    {
        if ((m_ic->duration == AV_NOPTS_VALUE) &&
            (!m_livetv && !m_ringBuffer->IsDisc()))
            av_estimate_timings(m_ic, 0);

        dur = m_ic->duration / (int64_t)AV_TIME_BASE;
    }

    if (dur > 0 && !m_livetv && !m_watchingRecording)
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
            float secs = m_ringBuffer->GetRealFileSize() * 1.0F / bytespersec;
            m_parent->SetFileLength((int)(secs),
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
        auto framenum = (long long)(total_secs * m_fps);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Chapter %1 found @ [%2:%3:%4]->%5")
                .arg(i + 1,   2,10,QChar('0'))
                .arg(hours,   2,10,QChar('0'))
                .arg(minutes, 2,10,QChar('0'))
                .arg(secs,    6,'f',3,QChar('0'))
                .arg(framenum));
    }

    if (getenv("FORCE_DTS_TIMESTAMPS"))
        m_forceDtsTimestamps = true;

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
    return m_recordingHasPositionMap;
}

float AvFormatDecoder::GetVideoFrameRate(AVStream *Stream, AVCodecContext *Context, bool Sanitise)
{
    double avg_fps = 0.0;
    double codec_fps = 0.0;
    double container_fps = 0.0;
    double estimated_fps = 0.0;

    if (Stream->avg_frame_rate.den && Stream->avg_frame_rate.num)
        avg_fps = av_q2d(Stream->avg_frame_rate); // MKV default_duration

    if (Context->time_base.den && Context->time_base.num) // tbc
        codec_fps = 1.0 / av_q2d(Context->time_base) / Context->ticks_per_frame;
    // Some formats report fps waaay too high. (wrong time_base)
    if (codec_fps > 121.0 && (Context->time_base.den > 10000) &&
        (Context->time_base.num == 1))
    {
        Context->time_base.num = 1001;  // seems pretty standard
        if (av_q2d(Context->time_base) > 0)
            codec_fps = 1.0 / av_q2d(Context->time_base);
    }
    if (Stream->time_base.den && Stream->time_base.num) // tbn
        container_fps = 1.0 / av_q2d(Stream->time_base);
    if (Stream->r_frame_rate.den && Stream->r_frame_rate.num) // tbr
        estimated_fps = av_q2d(Stream->r_frame_rate);

    // build a list of possible rates, best first
    vector<double> rates;

    // matroska demuxer sets the default_duration to avg_frame_rate
    // mov,mp4,m4a,3gp,3g2,mj2 demuxer sets avg_frame_rate
    if ((QString(m_ic->iformat->name).contains("matroska") ||
        QString(m_ic->iformat->name).contains("mov")) &&
        avg_fps < 121.0 && avg_fps > 3.0)
    {
        rates.emplace_back(avg_fps);
    }

    if (QString(m_ic->iformat->name).contains("avi") &&
        container_fps < 121.0 && container_fps > 3.0)
    {
        // avi uses container fps for timestamps
        rates.emplace_back(container_fps);
    }

    if (codec_fps < 121.0 && codec_fps > 3.0)
        rates.emplace_back(codec_fps);

    if (container_fps < 121.0 && container_fps > 3.0)
        rates.emplace_back(container_fps);

    if (avg_fps < 121.0 && avg_fps > 3.0)
        rates.emplace_back(avg_fps);

    // certain H.264 interlaced streams are detected at 2x using estimated (i.e. wrong)
    if (estimated_fps < 121.0 && estimated_fps > 3.0)
        rates.emplace_back(estimated_fps);

    // last resort
    rates.emplace_back(static_cast<float>(30000.0 / 1001.0));

    // debug
    if (!qFuzzyCompare(static_cast<float>(rates.front()), m_fps))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Selected FPS: %1 (Avg:%2 Mult:%3 Codec:%4 Container:%5 Estimated:%6)")
                .arg(static_cast<double>(rates.front())).arg(avg_fps)
                .arg(m_fpsMultiplier).arg(codec_fps).arg(container_fps).arg(estimated_fps));
    }

    auto IsStandard = [](double Rate)
    {
        // List of known, standards based frame rates
        static const vector<double> s_normRates =
        {
            24000.0 / 1001.0, 23.976, 25.0, 30000.0 / 1001.0,
            29.97, 30.0, 50.0, 60000.0 / 1001.0, 59.94, 60.0, 100.0,
            120000.0 / 1001.0, 119.88, 120.0
        };

        if (Rate > 1.0 && Rate < 121.0)
            for (auto rate : s_normRates)
                if (qFuzzyCompare(rate, Rate))
                    return true;
        return false;
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
                    if (qAbs(half - detected) < (half * 0.1))
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
                    H264Parser parser;
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
        default: break;
    }
    return 2;
}

void AvFormatDecoder::InitVideoCodec(AVStream *stream, AVCodecContext *enc,
                                     bool selectedStream)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InitVideoCodec ID:%1 Type:%2 Size:%3x%4")
            .arg(ff_codec_id_string(enc->codec_id))
            .arg(ff_codec_type_string(enc->codec_type))
            .arg(enc->width).arg(enc->height));

    if (m_ringBuffer && m_ringBuffer->IsDVD())
        m_directRendering = false;

    enc->opaque = static_cast<void*>(this);
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
        m_directRendering = true;

    // retrieve rotation information
    uint8_t* displaymatrix = av_stream_get_side_data(stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
    if (displaymatrix)
        m_videoRotation = static_cast<int>(-av_display_rotation_get(reinterpret_cast<int32_t*>(displaymatrix)));
    else
        m_videoRotation = 0;

    delete m_mythCodecCtx;
    m_mythCodecCtx = MythCodecContext::CreateContext(this, m_videoCodecId);
    m_mythCodecCtx->InitVideoCodec(enc, selectedStream, m_directRendering);
    if (m_mythCodecCtx->HwDecoderInit(enc) < 0)
    {
        // force it to switch to software decoding
        m_averrorCount = SEQ_PKT_ERR_MAX + 1;
        m_streamsChanged = true;
    }
    else
    {
        bool doublerate = m_parent->CanSupportDoubleRate();
        m_mythCodecCtx->SetDeinterlacing(enc, &m_videoDisplayProfile, doublerate);
    }

    if (codec1 && ((AV_CODEC_ID_MPEG2VIDEO == codec1->id) ||
                   (AV_CODEC_ID_MPEG1VIDEO == codec1->id)))
    {
        if (FlagIsSet(kDecodeFewBlocks))
        {
            int total_blocks = (enc->height + 15) / 16;
            enc->skip_top    = (total_blocks + 3) / 4;
            enc->skip_bottom = (total_blocks + 3) / 4;
        }

        if (FlagIsSet(kDecodeLowRes))
            enc->lowres = 2; // 1 = 1/2 size, 2 = 1/4 size
    }
    else if (codec1 && (AV_CODEC_ID_H264 == codec1->id) && FlagIsSet(kDecodeNoLoopFilter))
    {
        enc->flags &= ~AV_CODEC_FLAG_LOOP_FILTER;
        enc->skip_loop_filter = AVDISCARD_ALL;
    }

    if (FlagIsSet(kDecodeNoDecode))
        enc->skip_idct = AVDISCARD_ALL;

    if (selectedStream)
    {
        // m_fps is now set 'correctly' in ScanStreams so this additional call
        // to GetVideoFrameRate may now be redundant
        m_fps = GetVideoFrameRate(stream, enc, true);
        QSize dim    = get_video_dim(*enc);
        int   width  = m_currentWidth  = dim.width();
        int   height = m_currentHeight = dim.height();
        m_currentAspect = get_aspect(*enc);

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
        const AVCodec *codec2 = enc->codec;
        QString codecName;
        if (codec2)
            codecName = codec2->name;
        m_parent->SetVideoParams(width, height, static_cast<double>(m_fps),
                                 m_currentAspect, false, GetMaxReferenceFrames(enc),
                                 kScan_Detect, codecName);
        if (LCD *lcd = LCD::Get())
        {
            LCDVideoFormatSet video_format = VIDEO_MPG;

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
    memset(m_ccX08InPmt, 0, sizeof(m_ccX08InPmt));
    m_pmtTracks.clear();
    m_pmtTrackTypes.clear();

    // Figure out languages of ATSC captions
    if (!m_ic->cur_pmt_sect)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            "ScanATSCCaptionStreams() called with no PMT");
        return;
    }

    const ProgramMapTable pmt(PSIPTable(m_ic->cur_pmt_sect));

    uint i = 0;
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

    for (auto & desc : desc_list)
    {
        const CaptionServiceDescriptor csd(desc);
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
                m_ccX08InPmt[key] = true;
                m_pmtTracks.push_back(si);
                m_pmtTrackTypes.push_back(kTrackTypeCC708);
            }
            else
            {
                int line21 = csd.Line21Field(k) ? 3 : 1;
                StreamInfo si(av_index, lang, 0/*lang_idx*/, line21, 0);
                m_ccX08InPmt[line21-1] = true;
                m_pmtTracks.push_back(si);
                m_pmtTrackTypes.push_back(kTrackTypeCC608);
            }
        }
    }
}

void AvFormatDecoder::UpdateATSCCaptionTracks(void)
{
    m_tracks[kTrackTypeCC608].clear();
    m_tracks[kTrackTypeCC708].clear();
    memset(m_ccX08InTracks, 0, sizeof(m_ccX08InTracks));

    uint pidx = 0;
    uint sidx = 0;
    map<int,uint> lang_cc_cnt[2];
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

        for (const auto *desc : desc_list)
        {
            const TeletextDescriptor td(desc);
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

    if (m_ringBuffer && m_ringBuffer->IsDVD() &&
        m_ringBuffer->DVD()->AudioStreamsChanged())
    {
        m_ringBuffer->DVD()->AudioStreamsChanged(false);
        RemoveAudioStreams();
    }

    for (uint strm = 0; strm < m_ic->nb_streams; strm++)
    {
        AVCodecParameters *par = m_ic->streams[strm]->codecpar;
        AVCodecContext *enc = nullptr;

        QString codectype(ff_codec_type_string(par->codec_type));
        if (par->codec_type == AVMEDIA_TYPE_VIDEO)
            codectype += QString("(%1x%2)").arg(par->width).arg(par->height);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stream #%1: ID: 0x%2 Codec ID: %3 Type: %4 Bitrate: %5")
                .arg(strm).arg(static_cast<uint64_t>(m_ic->streams[strm]->id), 0, 16)
                .arg(ff_codec_id_string(par->codec_id))
                .arg(codectype).arg(par->bit_rate));

        switch (par->codec_type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                // reset any potentially errored hardware decoders
                if (m_resetHardwareDecoders)
                {
                    if (gCodecMap->hasCodecContext(m_ic->streams[strm]))
                    {
                        AVCodecContext* ctx = gCodecMap->getCodecContext(m_ic->streams[strm]);
                        if (ctx && (ctx->hw_frames_ctx || ctx->hw_device_ctx))
                            gCodecMap->freeCodecContext(m_ic->streams[strm]);
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
                            .arg(reinterpret_cast<unsigned long long>(enc), 0, 16)
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
                    ScanTeletextCaptions(static_cast<int>(strm));
                if (par->codec_id == AV_CODEC_ID_TEXT)
                    ScanRawTextCaptions(static_cast<int>(strm));
                m_bitrate += par->bit_rate;

                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("subtitle codec (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_DATA:
            {
                ScanTeletextCaptions(static_cast<int>(strm));
                m_bitrate += par->bit_rate;
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("data codec (%1)")
                        .arg(ff_codec_type_string(par->codec_type)));
                break;
            }
            case AVMEDIA_TYPE_ATTACHMENT:
            {
                if (par->codec_id == AV_CODEC_ID_TTF)
                   m_tracks[kTrackTypeAttachment].push_back(
                       StreamInfo(static_cast<int>(strm), 0, 0, m_ic->streams[strm]->id, 0));
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
                void *opaque = nullptr;
                const AVCodec *p = av_codec_iterate(&opaque);
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
                    p = av_codec_iterate(&opaque);
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
            uint lang_indx = lang_sub_cnt[lang]++;
            subtitleStreamCount++;

            m_tracks[kTrackTypeSubtitle].push_back(
                StreamInfo(static_cast<int>(strm), lang, lang_indx, m_ic->streams[strm]->id, 0, 0, false, false, forced));

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
            uint lang_indx = lang_aud_cnt[lang]++;
            audioStreamCount++;

            if (enc->avcodec_dual_language)
            {
                m_tracks[kTrackTypeAudio].push_back(
                    StreamInfo(static_cast<int>(strm), lang, lang_indx, m_ic->streams[strm]->id, channels,
                               false, false, false, type));
                lang_indx = lang_aud_cnt[lang]++;
                m_tracks[kTrackTypeAudio].push_back(
                    StreamInfo(static_cast<int>(strm), lang, lang_indx, m_ic->streams[strm]->id, channels,
                               true, false, false, type));
            }
            else
            {
                int logical_stream_id = 0;
                if (m_ringBuffer && m_ringBuffer->IsDVD())
                {
                    logical_stream_id = m_ringBuffer->DVD()->GetAudioTrackNum(m_ic->streams[strm]->id);
                    channels = m_ringBuffer->DVD()->GetNumAudioChannels(logical_stream_id);
                }
                else
                    logical_stream_id = m_ic->streams[strm]->id;

                if (logical_stream_id == -1)
                {
                    // This stream isn't mapped, so skip it
                    continue;
                }

                m_tracks[kTrackTypeAudio].push_back(
                   StreamInfo(static_cast<int>(strm), lang, lang_indx, logical_stream_id, channels,
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
    m_resetHardwareDecoders = false;
    if (!novideo && m_ic)
    {
        for(;;)
        {
            AVCodec *codec = nullptr;
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
            int selTrack = av_find_best_stream(m_ic, AVMEDIA_TYPE_VIDEO,
                                               -1, -1, &codec, 0);

            if (selTrack < 0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "No video track found/selected.");
                break;
            }

            AVStream *stream = m_ic->streams[selTrack];
            if (m_averrorCount > SEQ_PKT_ERR_MAX)
                gCodecMap->freeCodecContext(stream);
            AVCodecContext *enc = gCodecMap->getCodecContext(stream, codec);
            StreamInfo si(selTrack, 0, 0, 0, 0);

            m_tracks[kTrackTypeVideo].push_back(si);
            m_selectedTrack[kTrackTypeVideo] = si;

            QString codectype(ff_codec_type_string(enc->codec_type));
            if (enc->codec_type == AVMEDIA_TYPE_VIDEO)
                codectype += QString("(%1x%2)").arg(enc->width).arg(enc->height);
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Selected track #%1: ID: 0x%2 Codec ID: %3 Profile: %4 Type: %5 Bitrate: %6")
                    .arg(selTrack).arg(static_cast<uint64_t>(stream->id), 0, 16)
                    .arg(ff_codec_id_string(enc->codec_id))
                    .arg(avcodec_profile_name(enc->codec_id, enc->profile))
                    .arg(codectype).arg(enc->bit_rate));

            // If ScanStreams has been called on a stream change triggered by a
            // decoder error - because the decoder does not handle resolution
            // changes gracefully (NVDEC and maybe MediaCodec) - then the stream/codec
            // will still contain the old resolution but the AVCodecContext will
            // have been updated. This causes mayhem for a second or two.
            if ((enc->width != stream->codecpar->width) || (enc->height != stream->codecpar->height))
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString(
                    "Video resolution mismatch: Context: %1x%2 Stream: %3x%4 Codec: %5 Stream change: %6")
                    .arg(enc->width).arg(enc->height)
                    .arg(stream->codecpar->width).arg(stream->codecpar->height)
                    .arg(get_decoder_name(m_videoCodecId)).arg(m_streamsChanged));
            }

            delete m_privateDec;
            m_privateDec = nullptr;
            m_h264Parser->Reset();

            QSize dim = get_video_dim(*enc);
            int width  = max(dim.width(),  16);
            int height = max(dim.height(), 16);
            QString dec = "ffmpeg";
            uint thread_count = 1;
            QString codecName;
            if (enc->codec)
                codecName = enc->codec->name;
            // framerate appears to never be set - which is probably why
            // GetVideoFrameRate never uses it:)
            // So fallback to the GetVideoFrameRate call which should then ensure
            // the video display profile gets an accurate frame rate - instead of 0
            if (enc->framerate.den && enc->framerate.num)
                m_fps = float(enc->framerate.num) / float(enc->framerate.den);
            else
                m_fps = GetVideoFrameRate(stream, enc, true);

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
                        enc->skip_loop_filter = AVDISCARD_NONKEY;
                }

                m_videoCodecId = kCodec_NONE;
                uint version = mpeg_version(enc->codec_id);
                if (version)
                    m_videoCodecId = static_cast<MythCodecID>(kCodec_MPEG1 + version - 1);

                if (version && allowgpu && dec != "ffmpeg")
                {
                    // We need to set this so that MythyCodecContext can callback
                    // to the player in use to check interop support.
                    enc->opaque = static_cast<void*>(this);
                    MythCodecID hwcodec = MythCodecContext::FindDecoder(dec, stream, &enc, &codec);
                    if (hwcodec != kCodec_NONE)
                    {
                        // the context may have changed
                        enc->opaque = static_cast<void*>(this);
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

                // Use a PrivateDecoder if allowed in playerFlags AND matched
                // via the decoder name
                //m_privateDec = PrivateDecoder::Create(dec, m_playerFlags, enc);
                //if (m_privateDec)
                //    thread_count = 1;

                break;
            }

            // N.B. MediaCodec and NVDEC require frame timing
            m_useFrameTiming = false;
            if ((!m_ringBuffer->IsDVD() && (codec_sw_copy(m_videoCodecId) || codec_is_v4l2(m_videoCodecId))) ||
                (codec_is_nvdec(m_videoCodecId) || codec_is_mediacodec(m_videoCodecId) || codec_is_mediacodec_dec(m_videoCodecId)))
            {
                m_useFrameTiming = true;
            }

            if (FlagIsSet(kDecodeSingleThreaded))
                thread_count = 1;

            if (HAVE_THREADS)
            {
                // All of our callbacks are thread safe. This should improve
                // concurrency with software decode
                enc->thread_safe_callbacks = 1;

                // Only use a single thread for hardware decoding. There is no
                // performance improvement with multithreaded hardware decode
                // and asynchronous callbacks create issues with decoders that
                // use AVHWFrameContext where we need to release video resources
                // before they are recreated
                if (!foundgpudecoder)
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using %1 CPUs for decoding")
                        .arg(HAVE_THREADS ? thread_count : 1));
                    enc->thread_count = static_cast<int>(thread_count);
                }
            }

            InitVideoCodec(stream, enc, true);

            ScanATSCCaptionStreams(selTrack);
            UpdateATSCCaptionTracks();

            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Using %1 for video decoding").arg(GetCodecDecoderName()));
            {
                QMutexLocker locker(avcodeclock);
                m_mythCodecCtx->SetDecoderOptions(enc, codec);
                if (!OpenAVCodec(enc, codec))
                {
                    scanerror = -1;
                    break;
                }
            }
            break;
        }
    }

    if (m_ic && (static_cast<uint>(m_ic->bit_rate) > m_bitrate))
        m_bitrate = static_cast<uint>(m_ic->bit_rate);

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
    for (const auto & si : m_tracks[kTrackTypeTeletextCaptions])
    {
        if (si.m_language_index == lang_idx)
        {
             return si.m_language;
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
int AvFormatDecoder::GetCaptionLanguage(TrackType trackType, int service_num)
{
    int ret = -1;
    for (uint i = 0; i < (uint) m_pmtTrackTypes.size(); i++)
    {
        if ((m_pmtTrackTypes[i] == trackType) &&
            (m_pmtTracks[i].m_stream_id == service_num))
        {
            ret = m_pmtTracks[i].m_language;
            if (!iso639_is_key_undefined(ret))
                return ret;
        }
    }

    for (uint i = 0; i < (uint) m_streamTrackTypes.size(); i++)
    {
        if ((m_streamTrackTypes[i] == trackType) &&
            (m_streamTracks[i].m_stream_id == service_num))
        {
            ret = m_streamTracks[i].m_language;
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
    auto *decoder = static_cast<AvFormatDecoder*>(c->opaque);
    VideoFrameType type = PixelFormatToFrameType(c->pix_fmt);
    VideoFrameType* supported = decoder->GetPlayer()->DirectRenderFormats();
    bool found = false;
    while (*supported != FMT_NONE)
    {
        if (*supported == type)
        {
            found = true;
            break;
        }
        supported++;
    }

    if (!found)
    {
        decoder->m_directRendering = false;
        return avcodec_default_get_buffer2(c, pic, flags);
    }

    decoder->m_directRendering = true;
    VideoFrame *frame = decoder->GetPlayer()->GetNextVideoFrame();
    if (!frame)
        return -1;

    // We pre-allocate frames to certain alignments. If the coded size differs from
    // those alignments then re-allocate the frame. Changes in frame type (e.g.
    // YV12 to NV12) are always reallocated.
    int width  = (frame->width + MYTH_WIDTH_ALIGNMENT - 1) & ~(MYTH_WIDTH_ALIGNMENT - 1);
    int height = (frame->height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT - 1);

    if ((frame->codec != type) || (pic->width > width) || (pic->height > height))
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

    frame->colorshifted = 0;
    uint max = planes(frame->codec);
    for (uint i = 0; i < 3; i++)
    {
        pic->data[i]     = (i < max) ? (frame->buf + frame->offsets[i]) : nullptr;
        pic->linesize[i] = frame->pitches[i];
    }

    pic->opaque = frame;
    pic->reordered_opaque = c->reordered_opaque;

    // Set release method
    AVBufferRef *buffer = av_buffer_create(reinterpret_cast<uint8_t*>(frame), 0,
                            [](void* Opaque, uint8_t* Data)
                                {
                                    auto *avfd = static_cast<AvFormatDecoder*>(Opaque);
                                    auto *vf = reinterpret_cast<VideoFrame*>(Data);
                                    if (avfd && avfd->GetPlayer())
                                        avfd->GetPlayer()->DeLimboFrame(vf);
                                }
                            , decoder, 0);
    pic->buf[0] = buffer;

    return 0;
}

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

    bool had_608 = false;
    bool had_708 = false;
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
            uint field = 0;
            if (cc_type == 0x2)
            {
                // SCTE repeated field
                field = (m_lastScteField == 0 ? 1 : 0);
                m_invertScteField = (m_invertScteField == 0 ? 1 : 0);
            }
            else
            {
                field = cc_type ^ m_invertScteField;
            }

            if (cc608_good_parity(m_cc608ParityTable, data))
            {
                // in film mode, we may start at the wrong field;
                // correct if XDS start/cont/end code is detected
                // (must be field 2)
                if (scte && field == 0 &&
                    (data1 & 0x7f) <= 0x0f && (data1 & 0x7f) != 0x00)
                {
                    if (cc_type == 1)
                        m_invertScteField = 0;
                    field = 1;

                    // flush decoder
                    m_ccd608->FormatCC(0, -1, -1);
                }

                had_608 = true;
                m_ccd608->FormatCCField(m_lastCcPtsu / 1000, field, data);

                m_lastScteField = field;
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
            need_change_608 |= (seen_608[i] && !m_ccX08InTracks[i]) ||
                (!seen_608[i] && m_ccX08InTracks[i] && !m_ccX08InPmt[i]);
        }
    }

    bool need_change_708 = false;
    bool seen_708[64];
    if (check_708 || need_change_608)
    {
        m_ccd708->services(15/*seconds*/, seen_708);
        for (uint i = 1; i < 64 && !need_change_608 && !need_change_708; i++)
        {
            need_change_708 |= (seen_708[i] && !m_ccX08InTracks[i+4]) ||
                (!seen_708[i] && m_ccX08InTracks[i+4] && !m_ccX08InPmt[i+4]);
        }
        if (need_change_708 && !check_608)
            m_ccd608->GetServices(15/*seconds*/, seen_608);
    }

    if (!need_change_608 && !need_change_708)
        return;

    ScanATSCCaptionStreams(m_selectedTrack[kTrackTypeVideo].m_av_stream_index);

    m_streamTracks.clear();
    m_streamTrackTypes.clear();
    int av_index = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    int lang = iso639_str3_to_key("und");
    for (uint i = 1; i < 64; i++)
    {
        if (seen_708[i] && !m_ccX08InPmt[i+4])
        {
            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i, 0, false/*easy*/, true/*wide*/);
            m_streamTracks.push_back(si);
            m_streamTrackTypes.push_back(kTrackTypeCC708);
        }
    }
    for (uint i = 0; i < 4; i++)
    {
        if (seen_608[i] && !m_ccX08InPmt[i])
        {
            if (0==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 1);
            else if (2==i)
                lang = GetCaptionLanguage(kTrackTypeCC708, 2);
            else
                lang = iso639_str3_to_key("und");

            StreamInfo si(av_index, lang, 0/*lang_idx*/,
                          i+1, 0, false/*easy*/, false/*wide*/);
            m_streamTracks.push_back(si);
            m_streamTrackTypes.push_back(kTrackTypeCC608);
        }
    }
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
            m_maxKeyframeDist = max(m_keyframeDist, m_maxKeyframeDist);

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
            float secs = m_ringBuffer->GetRealFileSize() * 1.0F / bytespersec;
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
        bufptr = avpriv_find_start_code(bufptr, bufend, &m_startCodeState);

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
                if (m_privateDec)
                    m_privateDec->Reset();

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
                m_firstVPts = m_lastAPts = m_lastVPts = m_lastCcPtsu = 0;
                m_firstVPtsInuse = true;
                m_faultyPts = m_faultyDts = 0;
                m_lastPtsForFaultDetection = 0;
                m_lastDtsForFaultDetection = 0;
                m_ptsDetected = false;
                m_reorderedPtsDetected = false;

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
int AvFormatDecoder::H264PreProcessPkt(AVStream *stream, AVPacket *pkt)
{
    AVCodecContext *context = gCodecMap->getCodecContext(stream);
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
        buf += m_h264Parser->addBytes(buf, static_cast<unsigned int>(buf_end - buf), 0);

        if (m_h264Parser->stateChanged())
        {
            if (m_h264Parser->FieldType() != H264Parser::FIELD_BOTTOM)
            {
                if (m_h264Parser->onFrameStart())
                    ++num_frames;

                if (!m_h264Parser->onKeyFrameStart())
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
        float aspect = get_aspect(*m_h264Parser);
        int width  = static_cast<int>(m_h264Parser->pictureWidthCropped());
        int height = static_cast<int>(m_h264Parser->pictureHeightCropped());
        double seqFPS = m_h264Parser->frameRate();

        bool res_changed = ((width  != m_currentWidth) || (height != m_currentHeight));
        bool fps_changed = (seqFPS > 0.0) && ((seqFPS > static_cast<double>(m_fps) + 0.01) ||
                                              (seqFPS < static_cast<double>(m_fps) - 0.01));
        bool forcechange = !qFuzzyCompare(aspect + 10.0F, m_currentAspect) &&
                            m_mythCodecCtx && m_mythCodecCtx->DecoderWillResetOnAspect();
        m_currentAspect = aspect;

        if (fps_changed || res_changed || forcechange)
        {
            if (m_privateDec)
                m_privateDec->Reset();

            // N.B. we now set the default scan to kScan_Ignore as interlaced detection based on frame
            // size and rate is extremely error prone and FFmpeg gets it right far more often.
            // N.B. if a decoder deinterlacer is in use - the stream must be progressive
            bool doublerate = false;
            bool decoderdeint = m_mythCodecCtx ? m_mythCodecCtx->IsDeinterlacing(doublerate, true) : false;
            m_parent->SetVideoParams(width, height, seqFPS, m_currentAspect, forcechange,
                                     static_cast<int>(m_h264Parser->getRefFrames()),
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
            m_firstVPts = m_lastAPts = m_lastVPts = m_lastCcPtsu = 0;
            m_firstVPtsInuse = true;
            m_faultyPts = m_faultyDts = 0;
            m_lastPtsForFaultDetection = 0;
            m_lastDtsForFaultDetection = 0;
            m_ptsDetected = false;
            m_reorderedPtsDetected = false;

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
        AVRational pkt_dur = AVRationalInit(pkt->duration);
        pkt_dur = av_mul_q(pkt_dur, curstream->time_base);
        if (pkt_dur.num == 1501 && pkt_dur.den == 90000)
            pkt_dur = AVRationalInit(1001, 60000); // 1501.5/90000
        m_totalDuration = av_add_q(m_totalDuration, pkt_dur);
    }

    m_justAfterChange = false;

    if (m_exitAfterDecoded)
        m_gotVideoFrame = true;

    return true;
}

bool AvFormatDecoder::ProcessVideoPacket(AVStream *curstream, AVPacket *pkt, bool &Retry)
{
    int ret = 0;
    int gotpicture = 0;
    AVCodecContext *context = gCodecMap->getCodecContext(curstream);
    MythAVFrame mpa_pic;
    if (!mpa_pic)
        return false;
    mpa_pic->reordered_opaque = AV_NOPTS_VALUE;

    if (pkt->pts != AV_NOPTS_VALUE)
        m_ptsDetected = true;

    bool sentPacket = false;
    int ret2 = 0;
    avcodeclock->lock();
    if (m_privateDec)
    {
        if (QString(m_ic->iformat->name).contains("avi") || !m_ptsDetected)
            pkt->pts = pkt->dts;
        // TODO disallow private decoders for dvd playback
        // N.B. we do not reparse the frame as it breaks playback for
        // everything but libmpeg2
        ret = m_privateDec->GetFrame(curstream, mpa_pic, &gotpicture, pkt);
        sentPacket = true;
    }
    else
    {
        if (!m_useFrameTiming)
            context->reordered_opaque = pkt->pts;

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
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("video avcodec_send_packet error: %1 (%2) gotpicture:%3")
                .arg(av_make_error_string(error, sizeof(error), ret2))
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
                .arg(pkt->pts).arg(mpa_pic->pts).arg(pkt->pts)
                .arg(mpa_pic->pkt_dts));

        if (!m_useFrameTiming)
        {
            int64_t pts = 0;

            // Detect faulty video timestamps using logic from ffplay.
            if (pkt->dts != AV_NOPTS_VALUE)
            {
                m_faultyDts += (pkt->dts <= m_lastDtsForFaultDetection);
                m_lastDtsForFaultDetection = pkt->dts;
            }
            if (mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
            {
                m_faultyPts += (mpa_pic->reordered_opaque <= m_lastPtsForFaultDetection);
                m_lastPtsForFaultDetection = mpa_pic->reordered_opaque;
                m_reorderedPtsDetected = true;
            }

            // Explicity use DTS for DVD since they should always be valid for every
            // frame and fixups aren't enabled for DVD.
            // Select reordered_opaque (PTS) timestamps if they are less faulty or the
            // the DTS timestamp is missing. Also use fixups for missing PTS instead of
            // DTS to avoid oscillating between PTS and DTS. Only select DTS if PTS is
            // more faulty or never detected.
            if (m_forceDtsTimestamps || m_ringBuffer->IsDVD())
            {
                if (pkt->dts != AV_NOPTS_VALUE)
                    pts = pkt->dts;
                m_ptsSelected = false;
            }
            else if (m_privateDec && m_privateDec->NeedsReorderedPTS() &&
                    mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
            {
                pts = mpa_pic->reordered_opaque;
                m_ptsSelected = true;
            }
            else if (m_faultyPts <= m_faultyDts && m_reorderedPtsDetected)
            {
                if (mpa_pic->reordered_opaque != AV_NOPTS_VALUE)
                    pts = mpa_pic->reordered_opaque;
                m_ptsSelected = true;
            }
            else if (pkt->dts != AV_NOPTS_VALUE)
            {
                pts = pkt->dts;
                m_ptsSelected = false;
            }

            LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_DEBUG, LOC +
                QString("video packet timestamps reordered %1 pts %2 dts %3 (%4)")
                    .arg(mpa_pic->reordered_opaque).arg(pkt->pts).arg(pkt->dts)
                    .arg((m_forceDtsTimestamps) ? "dts forced" :
                        (m_ptsSelected) ? "reordered" : "dts"));

            mpa_pic->reordered_opaque = pts;
        }
        ProcessVideoFrame(curstream, mpa_pic);
    }

    if (!sentPacket)
    {
        // MythTV logic expects that only one frame is processed
        // Save the packet for later and return.
        auto *newPkt = new AVPacket;
        memset(newPkt, 0, sizeof(AVPacket));
        av_init_packet(newPkt);
        av_packet_ref(newPkt, pkt);
        m_storedPackets.prepend(newPkt);
    }
    return true;
}

bool AvFormatDecoder::ProcessVideoFrame(AVStream *Stream, AVFrame *AvFrame)
{

    AVCodecContext *context = gCodecMap->getCodecContext(Stream);

    // We need to mediate between ATSC and SCTE data when both are present.  If
    // both are present, we generally want to prefer ATSC.  However, there may
    // be large sections of the recording where ATSC is used and other sections
    // where SCTE is used.  In that case, we want to allow a natural transition
    // from ATSC back to SCTE.  We do this by allowing 10 consecutive SCTE
    // frames, without an intervening ATSC frame, to cause a switch back to
    // considering SCTE frames.  The number 10 is somewhat arbitrarily chosen.

    uint cc_len = static_cast<uint>(max(AvFrame->scte_cc_len,0));
    uint8_t *cc_buf = AvFrame->scte_cc_buf;
    bool scte = true;

    // If we saw SCTE, then decrement a nonzero ignore_scte count.
    if (cc_len > 0 && m_ignoreScte)
        --m_ignoreScte;

    // If both ATSC and SCTE caption data are available, prefer ATSC
    if ((AvFrame->atsc_cc_len > 0) || m_ignoreScte)
    {
        cc_len = static_cast<uint>(max(AvFrame->atsc_cc_len, 0));
        cc_buf = AvFrame->atsc_cc_buf;
        scte = false;
        // If we explicitly saw ATSC, then reset ignore_scte count.
        if (cc_len > 0)
            m_ignoreScte = 10;
    }

    // Decode CEA-608 and CEA-708 captions
    for (uint i = 0; i < cc_len; i += ((cc_buf[i] & 0x1f) * 3) + 2)
        DecodeDTVCC(cc_buf + i, cc_len - i, scte);

    if (cc_len == 0) {
        // look for A53 captions
        AVFrameSideData *side_data = av_frame_get_side_data(AvFrame, AV_FRAME_DATA_A53_CC);
        if (side_data && (side_data->size > 0)) {
            DecodeCCx08(side_data->data, static_cast<uint>(side_data->size), false);
        }
    }

    auto *frame = static_cast<VideoFrame*>(AvFrame->opaque);
    if (frame)
        frame->directrendering = m_directRendering;

    if (FlagIsSet(kDecodeNoDecode))
    {
        // Do nothing, we just want the pts, captions, subtites, etc.
        // So we can release the unconverted blank video frame to the
        // display queue.
        if (frame)
            frame->directrendering = 0;
    }
    else if (!m_directRendering)
    {
        VideoFrame *oldframe = frame;
        frame = m_parent->GetNextVideoFrame();
        frame->directrendering = 0;

        if (!m_mythCodecCtx->RetrieveFrame(context, frame, AvFrame))
        {
            AVFrame tmppicture;
            av_image_fill_arrays(tmppicture.data, tmppicture.linesize,
                                 frame->buf, AV_PIX_FMT_YUV420P, AvFrame->width,
                                 AvFrame->height, IMAGE_ALIGN);
            tmppicture.data[0] = frame->buf + frame->offsets[0];
            tmppicture.data[1] = frame->buf + frame->offsets[1];
            tmppicture.data[2] = frame->buf + frame->offsets[2];
            tmppicture.linesize[0] = frame->pitches[0];
            tmppicture.linesize[1] = frame->pitches[1];
            tmppicture.linesize[2] = frame->pitches[2];

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
            oldframe->pause_frame = 0;
            oldframe->interlaced_frame = AvFrame->interlaced_frame;
            oldframe->top_field_first = AvFrame->top_field_first;
            oldframe->interlaced_reversed = 0;
            oldframe->new_gop = 0;
            oldframe->colorspace = AvFrame->colorspace;
            oldframe->colorrange = AvFrame->color_range;
            oldframe->colorprimaries = AvFrame->color_primaries;
            oldframe->colortransfer = AvFrame->color_trc;
            oldframe->chromalocation = AvFrame->chroma_location;
            oldframe->frameNumber = m_framesPlayed;
            oldframe->frameCounter = m_frameCounter++;
            oldframe->aspect = m_currentAspect;
            oldframe->deinterlace_inuse = DEINT_NONE;
            oldframe->deinterlace_inuse2x = 0;
            oldframe->already_deinterlaced = 0;
            oldframe->rotation = m_videoRotation;
            m_parent->DiscardVideoFrame(oldframe);
        }
    }

    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "NULL videoframe - direct rendering not "
                                       "correctly initialized.");
        return false;
    }

    long long pts = 0;
    if (m_useFrameTiming)
    {
        pts = AvFrame->pts;
        if (pts == AV_NOPTS_VALUE)
            pts = AvFrame->pkt_dts;
        if (pts == AV_NOPTS_VALUE)
            pts = AvFrame->reordered_opaque;
        if (pts == AV_NOPTS_VALUE)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "No PTS found - unable to process video.");
            return false;
        }
        pts = static_cast<long long>(av_q2d(Stream->time_base) * pts * 1000);
    }
    else
        pts = static_cast<long long>(av_q2d(Stream->time_base) * AvFrame->reordered_opaque * 1000);

    long long temppts = pts;
    // Validate the video pts against the last pts. If it's
    // a little bit smaller, equal or missing, compute
    // it from the last. Otherwise assume a wraparound.
    if (!m_ringBuffer->IsDVD() &&
        temppts <= m_lastVPts &&
        (temppts + (1000 / m_fps) > m_lastVPts || temppts <= 0))
    {
        temppts = m_lastVPts;
        temppts += static_cast<long long>(1000 / m_fps);
        // MPEG2/H264 frames can be repeated, update pts accordingly
        temppts += static_cast<long long>(AvFrame->repeat_pict * 500 / m_fps);
    }

    // Calculate actual fps from the pts values.
    long long ptsdiff = temppts - m_lastVPts;
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
            m_parent->SetFrameRate(static_cast<double>(m_fps));
    }

    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
        QString("video timecode %1 %2 %3 %4%5")
            .arg(m_useFrameTiming ? AvFrame->pts : AvFrame->reordered_opaque).arg(pts)
            .arg(temppts).arg(m_lastVPts)
            .arg((pts != temppts) ? " fixup" : ""));

    if (frame)
    {
        frame->interlaced_frame = AvFrame->interlaced_frame;
        frame->top_field_first  = AvFrame->top_field_first;
        frame->interlaced_reversed = 0;
        frame->new_gop          = m_nextDecodedFrameIsKeyFrame;
        frame->repeat_pict      = AvFrame->repeat_pict;
        frame->disp_timecode    = NormalizeVideoTimecode(Stream, temppts);
        frame->frameNumber      = m_framesPlayed;
        frame->frameCounter     = m_frameCounter++;
        frame->aspect           = m_currentAspect;
        frame->dummy            = 0;
        frame->pause_frame      = 0;
        frame->colorspace       = AvFrame->colorspace;
        frame->colorrange       = AvFrame->color_range;
        frame->colorprimaries   = AvFrame->color_primaries;
        frame->colortransfer    = AvFrame->color_trc;
        frame->chromalocation   = AvFrame->chroma_location;
        frame->pix_fmt          = AvFrame->format;
        frame->deinterlace_inuse = DEINT_NONE;
        frame->deinterlace_inuse2x = 0;
        frame->already_deinterlaced = 0;
        frame->rotation         = m_videoRotation;
        m_parent->ReleaseNextVideoFrame(frame, temppts);
        m_mythCodecCtx->PostProcessFrame(context, frame);
    }

    m_nextDecodedFrameIsKeyFrame = false;
    m_decodedVideoFrame = frame;
    m_gotVideoFrame = true;
    if (++m_fpsSkip >= m_fpsMultiplier)
    {
        ++m_framesPlayed;
        m_fpsSkip = 0;
    }

    m_lastVPts = temppts;
    if (!m_firstVPts && m_firstVPtsInuse)
        m_firstVPts = temppts;

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
    unsigned long long utc = m_lastCcPtsu;

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
                    if (cc608_good_parity(m_cc608ParityTable, data))
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
    int componentTag = 0;
    int dataBroadcastId = 0;
    unsigned carouselId = 0;
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
                QMutexLocker locker(avcodeclock);
                m_ringBuffer->DVD()->DecodeSubtitles(&subtitle, &gotSubtitles,
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
                m_allowedQuit |= (m_itv && m_itv->ImageHasChanged());
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
                    if (m_ringBuffer->IsDVD() || par->channels)
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

    if (m_ringBuffer->IsInDiscMenuOrStillFrame())
        return -1;

    return DecoderBase::AutoSelectTrack(type);
}

static vector<int> filter_lang(const sinfo_vec_t &tracks, int lang_key,
                               const vector<int> &ftype)
{
    vector<int> ret;

    for (int index : ftype)
    {
        if ((lang_key < 0) || tracks[index].m_language == lang_key)
            ret.push_back(index);
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
    int selectedTrack = -1;
    int max_seen = -1;

    for (int f : fs)
    {
        const int stream_index = tracks[f].m_av_stream_index;
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
            selectedTrack = f;
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
    for (const auto & track : atracks)
    {
        int idx = track.m_av_stream_index;
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
            m_lastAPts = (long long)(av_q2d(curstream->time_base) * pkt->pts * 1000);

        if (!m_useFrameTiming)
        {
            // This code under certain conditions causes jump backwards to lose
            // audio.
            if (m_skipAudio && m_selectedTrack[kTrackTypeVideo].m_av_stream_index > -1)
            {
                if ((m_lastAPts < m_lastVPts - (10.0F / m_fps)) || m_lastVPts == 0)
                    break;
                m_skipAudio = false;
            }

            // skip any audio frames preceding first video frame
            if (m_firstVPtsInuse && m_firstVPts && (m_lastAPts < m_firstVPts))
            {
                LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                    QString("discarding early audio timecode %1 %2 %3")
                        .arg(pkt->pts).arg(pkt->dts).arg(m_lastAPts));
                break;
            }
        }
        m_firstVPtsInuse = false;

        avcodeclock->lock();
        data_size = 0;

        // Check if the number of channels or sampling rate have changed
        if (ctx->sample_rate != m_audioOut.m_sampleRate ||
            ctx->channels    != m_audioOut.m_channels ||
            AudioOutputSettings::AVSampleFormatToFormat(ctx->sample_fmt,
                                                        ctx->bits_per_raw_sample) != m_audioOut.format)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Audio stream changed");
            if (ctx->channels != m_audioOut.m_channels)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Number of audio channels changed from %1 to %2")
                    .arg(m_audioOut.m_channels).arg(ctx->channels));
            }
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

        long long temppts = m_lastAPts;

        if (audSubIdx != -1 && !m_audioOut.m_doPassthru)
            extract_mono_channel(audSubIdx, &m_audioOut,
                                 (char *)m_audioSamples, data_size);

        int samplesize = AudioOutputSettings::SampleSize(m_audio->GetFormat());
        int frames = (ctx->channels <= 0 || decoded_size < 0 || !samplesize) ? -1 :
            decoded_size / (ctx->channels * samplesize);
        m_audio->AddAudioData((char *)m_audioSamples, data_size, temppts, frames);
        if (m_audioOut.m_doPassthru && !m_audio->NeedDecodingBeforePassthrough())
        {
            m_lastAPts += m_audio->LengthLastData();
        }
        else
        {
            m_lastAPts += (long long)
                ((double)(frames * 1000) / ctx->sample_rate);
        }

        LOG(VB_TIMESTAMP, LOG_INFO, LOC + QString("audio timecode %1 %2 %3 %4")
                .arg(pkt->pts).arg(pkt->dts).arg(temppts).arg(m_lastAPts));

        m_allowedQuit |= m_ringBuffer->IsInStillFrame() ||
                       m_audio->IsBufferAlmostFull();

        tmp_pkt.data += ret;
        tmp_pkt.size -= ret;
        firstloop = false;
    }

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

    avcodeclock->lock();
    AutoSelectTracks();
    avcodeclock->unlock();

    m_skipAudio = (m_lastVPts == 0);

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
        m_skipAudio = false;
    }

    m_allowedQuit = m_audio->IsBufferAlmostFull();

    if (m_privateDec && m_privateDec->HasBufferedFrames() &&
       (m_selectedTrack[kTrackTypeVideo].m_av_stream_index > -1))
    {
        int got_picture  = 0;
        AVStream *stream = m_ic->streams[m_selectedTrack[kTrackTypeVideo]
                                 .m_av_stream_index];
        MythAVFrame mpa_pic;
        if (!mpa_pic)
            return false;

        m_privateDec->GetFrame(stream, mpa_pic, &got_picture, nullptr);
        if (got_picture)
            ProcessVideoFrame(stream, mpa_pic);
    }

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
                     (m_storedPackets.count() < max_video_queue_size) &&
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
                if (m_storedPackets.count() >= max_video_queue_size)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        QString("Audio %1 ms behind video but already %2 "
                                "video frames queued. AV-Sync might be broken.")
                            .arg(m_lastVPts-m_lastAPts).arg(m_storedPackets.count()));
                }
                m_allowedQuit = true;
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
                    m_lastCcPtsu = (long long)
                                 (av_q2d(curstream->time_base)*pkt->pts*1000000);
                }

                if (!(decodetype & kDecodeVideo))
                {
                    m_framesPlayed++;
                    m_gotVideoFrame = true;
                    break;
                }

                if (!ProcessVideoPacket(curstream, pkt, Retry))
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

        if (!have_err && !Retry)
            m_frameDecoded = 1;
        av_packet_unref(pkt);
        if (Retry)
            break;
    }

    delete pkt;
    return true;
}

void AvFormatDecoder::StreamChangeCheck(void)
{
    if (m_streamsChanged)
    {
        SeekReset(0, 0, true, true);
        avcodeclock->lock();
        ScanStreams(false);
        avcodeclock->unlock();
        m_streamsChanged = false;
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

        clear(frame);
        frame->dummy = 1;
        m_parent->ReleaseNextVideoFrame(frame, m_lastVPts);
        m_parent->DeLimboFrame(frame);

        frame->interlaced_frame = 0; // not interlaced
        frame->top_field_first  = 1; // top field first
        frame->interlaced_reversed = 0;
        frame->new_gop          = 0;
        frame->repeat_pict      = 0; // not a repeated picture
        frame->frameNumber      = m_framesPlayed;
        frame->frameCounter     = m_frameCounter++;
        frame->dummy            = 1;
        frame->pause_frame      = 0;
        frame->colorspace       = AVCOL_SPC_BT709;
        frame->colorrange       = AVCOL_RANGE_MPEG;
        frame->colorprimaries   = AVCOL_PRI_BT709;
        frame->colortransfer    = AVCOL_TRC_BT709;
        frame->chromalocation   = AVCHROMA_LOC_LEFT;
        frame->deinterlace_inuse = DEINT_NONE;
        frame->deinterlace_inuse2x = 0;
        frame->already_deinterlaced = 0;
        frame->rotation         = 0;

        m_decodedVideoFrame = frame;
        m_framesPlayed++;
        m_gotVideoFrame = true;
    }
    return true;
}

QString AvFormatDecoder::GetCodecDecoderName(void) const
{
    if (m_privateDec)
        return m_privateDec->GetName();
    return get_decoder_name(m_videoCodecId);
}

QString AvFormatDecoder::GetRawEncodingType(void)
{
    int stream = m_selectedTrack[kTrackTypeVideo].m_av_stream_index;
    if (stream < 0 || !m_ic)
        return QString();
    return ff_codec_id_string(m_ic->streams[stream]->codecpar->codec_id);
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
    bool passthru = false;

    // if withProfile == false, we will accept any DTS stream regardless
    // of its profile. We do so, so we can bitstream DTS-HD as DTS core
    if (!withProfile && par->codec_id == AV_CODEC_ID_DTS && !m_audio->CanDTSHD())
    {
        passthru = m_audio->CanPassthrough(par->sample_rate, par->channels,
                                           par->codec_id, FF_PROFILE_DTS);
    }
    else
    {
        passthru = m_audio->CanPassthrough(par->sample_rate, par->channels,
                                           par->codec_id, par->profile);
    }

    passthru &= !m_disablePassthru;

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
    int requested_channels = 0;

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
        int64_t filesize = 0;
        ic->duration = duration;
        if (ic->pb && (filesize = avio_size(ic->pb)) > 0) {
            /* compute the bitrate */
            ic->bit_rate = (double)filesize * 8.0 * AV_TIME_BASE /
                (double)ic->duration;
        }
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
