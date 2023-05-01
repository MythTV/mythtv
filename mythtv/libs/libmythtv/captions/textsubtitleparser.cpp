// -*- Mode: c++ -*-
/** TextSubtitles
 *  Copyright (c) 2006 by Pekka Jääskeläinen
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// ANSI C
#include <cstdio>
#include <cstring>
#include <climits>

// C++
#include <algorithm>

// Qt
#include <QRegularExpression>
#include <QRunnable>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif
#include <QFile>
#include <QDataStream>

// MythTV
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"
#include "libmythtv/io/mythmediabuffer.h"
#include "libmyth/mythaverror.h"

#include "captions/textsubtitleparser.h"

#define IO_BUFFER_SIZE 32768

// This background thread helper class is adapted from the
// RebuildSaver class in mythcommflagplayer.cpp.
class SubtitleLoadHelper : public QRunnable
{
  public:
    SubtitleLoadHelper(const QString &fileName,
                       TextSubtitles *target)
        : m_fileName(fileName), m_target(target)
    {
        QMutexLocker locker(&s_lock);
        ++s_loading[m_target];
    }

    void run(void) override // QRunnable
    {
        TextSubtitleParser::LoadSubtitles(m_fileName, *m_target, false);

        QMutexLocker locker(&s_lock);
        --s_loading[m_target];
        if (!s_loading[m_target])
            s_wait.wakeAll();
    }

    static bool IsLoading(TextSubtitles *target)
    {
        QMutexLocker locker(&s_lock);
        return s_loading[target] != 0U;
    }

    static void Wait(TextSubtitles *target)
    {
        QMutexLocker locker(&s_lock);
        if (!s_loading[target])
            return;
        while (s_wait.wait(&s_lock))
        {
            if (!s_loading[target])
                return;
        }
    }

  private:
    const QString &m_fileName;
    TextSubtitles *m_target;

    static QMutex                     s_lock;
    static QWaitCondition             s_wait;
    static QHash<TextSubtitles*,uint> s_loading;
};
QMutex                     SubtitleLoadHelper::s_lock;
QWaitCondition             SubtitleLoadHelper::s_wait;
QHash<TextSubtitles*,uint> SubtitleLoadHelper::s_loading;

// Work around the fact that RemoteFile doesn't work when the target
// file is actually local.
class RemoteFileWrapper
{
public:
    explicit RemoteFileWrapper(const QString &filename) {
        // This test stolen from FileRingBuffer::OpenFile()
        bool is_local =
            (!filename.startsWith("/dev")) &&
            ((filename.startsWith("/")) || QFile::exists(filename));
        m_isRemote = !is_local;
        if (m_isRemote)
        {
            m_localFile = nullptr;
            m_remoteFile = new RemoteFile(filename, false, false, 0s);
        }
        else
        {
            m_remoteFile = nullptr;
            m_localFile = new QFile(filename);
            if (!m_localFile->open(QIODevice::ReadOnly))
            {
                delete m_localFile;
                m_localFile = nullptr;
            }
        }
    }
    ~RemoteFileWrapper() {
        delete m_remoteFile;
        delete m_localFile;
    }

    RemoteFileWrapper(const RemoteFileWrapper &) = delete;            // not copyable
    RemoteFileWrapper &operator=(const RemoteFileWrapper &) = delete; // not copyable

    bool isOpen(void) const {
        if (m_isRemote)
            return m_remoteFile->isOpen();
        return m_localFile;
    }
    long long GetFileSize(void) const {
        if (m_isRemote)
            return m_remoteFile->GetFileSize();
        if (m_localFile)
            return m_localFile->size();
        return 0;
    }
    int Read(void *data, int size) {
        if (m_isRemote)
            return m_remoteFile->Read(data, size);
        if (m_localFile)
        {
            QDataStream stream(m_localFile);
            return stream.readRawData(static_cast<char*>(data), size);
        }
        return 0;
    }
private:
    bool m_isRemote;
    RemoteFile *m_remoteFile;
    QFile *m_localFile;
};

static bool operator<(const text_subtitle_t& left,
                      const text_subtitle_t& right)
{
    return left.m_start < right.m_start;
}

TextSubtitles::~TextSubtitles()
{
    SubtitleLoadHelper::Wait(this);
}

/** \fn TextSubtitles::HasSubtitleChanged(uint64_t timecode) const
 *  \brief Returns true in case the subtitle to display has changed
 *         since the last GetSubtitles() call.
 *
 *  This is used to avoid redisplaying subtitles that are already displaying.
 *
 *  \param timecode The timecode (frame number or time stamp)
 *         of the current video position.
 *  \return True in case new subtitles should be displayed.
 */
bool TextSubtitles::HasSubtitleChanged(uint64_t timecode) const
{
    return (timecode < m_lastReturnedSubtitle.m_start ||
            timecode > m_lastReturnedSubtitle.m_end);
}

/** \fn TextSubtitles::GetSubtitles(uint64_t timecode) const
 *  \brief Returns the subtitles to display at the given timecode.
 *
 *  \param timecode The timecode (frame number or time stamp) of the
 *         current video position.
 *  \return The subtitles as a list of strings.
 */
QStringList TextSubtitles::GetSubtitles(uint64_t timecode)
{
    if (!m_isInProgress && m_subtitles.empty())
        return {};

    text_subtitle_t searchTarget(timecode, timecode);

    auto nextSubPos =
        std::lower_bound(m_subtitles.begin(), m_subtitles.end(), searchTarget);

    uint64_t startCode = 0;
    uint64_t endCode = 0;
    if (nextSubPos != m_subtitles.begin())
    {
        auto currentSubPos = nextSubPos;
        --currentSubPos;

        const text_subtitle_t &sub = *currentSubPos;
        if (sub.m_start <= timecode && sub.m_end >= timecode)
        {
            // found a sub to display
            m_lastReturnedSubtitle = sub;
            return m_lastReturnedSubtitle.m_textLines;
        }

        // the subtitle time span has ended, let's display a blank sub
        startCode = sub.m_end + 1;
    }

    if (nextSubPos == m_subtitles.end())
    {
        if (m_isInProgress)
        {
            const int maxReloadInterval = 1000; // ms
            if (IsFrameBasedTiming())
            {
                // Assume conservative 24fps
                endCode = startCode + maxReloadInterval / 24;
            }
            else
            {
                endCode = startCode + maxReloadInterval;
            }
            QDateTime now = QDateTime::currentDateTimeUtc();
            if (!m_fileName.isEmpty() &&
                m_lastLoaded.msecsTo(now) >= maxReloadInterval)
            {
                TextSubtitleParser::LoadSubtitles(m_fileName, *this, true);
            }
        }
        else
        {
            // at the end of video, the blank subtitle should last
            // until forever
            endCode = startCode + INT_MAX;
        }
    }
    else
    {
        endCode = (*nextSubPos).m_start - 1;
    }

    // we are in a position in which there are no subtitles to display,
    // return an empty subtitle and create a dummy empty subtitle for this
    // time span so SubtitleChanged() functions also in this case
    text_subtitle_t blankSub(startCode, endCode);
    m_lastReturnedSubtitle = blankSub;

    return {};
}

void TextSubtitles::AddSubtitle(const text_subtitle_t &newSub)
{
    QMutexLocker locker(&m_lock);
    m_subtitles.push_back(newSub);
}

void TextSubtitles::Clear(void)
{
    QMutexLocker locker(&m_lock);
    m_subtitles.clear();
}

void TextSubtitles::SetLastLoaded(void)
{
    emit TextSubtitlesUpdated();
    QMutexLocker locker(&m_lock);
    m_lastLoaded = QDateTime::currentDateTimeUtc();
}

/// A local buffer that the entire file is slurped into.
struct local_buffer_t {
  char              *rbuffer_text;
  off_t              rbuffer_len;
  off_t              rbuffer_cur;
};

/// \brief Read data from the file buffer into the avio context buffer.
int TextSubtitleParser::read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    auto *bd = (struct local_buffer_t *)opaque;

    /* copy internal buffer data to buf */
    off_t remaining = bd->rbuffer_len - bd->rbuffer_cur;
    if (remaining <= 0)
        return AVERROR_EOF;
    buf_size = FFMIN(buf_size, remaining);
    memcpy(buf, bd->rbuffer_text + bd->rbuffer_cur, buf_size);
    bd->rbuffer_cur += buf_size;

    return buf_size;
}

///  \brief Seek in the file buffer.
int64_t TextSubtitleParser::seek_packet(void *opaque, int64_t offset, int whence)
{
    auto *bd = (struct local_buffer_t *)opaque;

    switch (whence)
    {
      case SEEK_CUR:
        offset = bd->rbuffer_cur + offset;
        break;

      case SEEK_END:
        offset = bd->rbuffer_len - offset;
        break;

      case SEEK_SET:
        break;

      default:
        return -1;
    }

    if ((offset < 0) || (offset > bd->rbuffer_len))
        return -1;
    bd->rbuffer_cur = offset;
    return 0;
}

/// \brief Decode a single packet worth of data.
///
/// av_read_frame guarantees that pkt->pts, pkt->dts and pkt->duration
/// are always set to correct values in AVStream.time_base units (and
/// guessed if the format cannot provide them). pkt->pts can be
/// AV_NOPTS_VALUE if the video format has B-frames, so it is better to
/// rely on pkt->dts if you do not decompress the payload.
int TextSubtitleParser::decode(TextSubtitles &target, AVCodecContext *dec_ctx,
                               AVStream *stream, AVPacket *pkt)
{
    AVSubtitle sub {};
    int got_sub_ptr {0};

    int ret = avcodec_decode_subtitle2(dec_ctx, &sub, &got_sub_ptr, pkt);
    if (ret < 0)
        return ret;
    if (!got_sub_ptr)
        return -1;

    long start = static_cast<long>(av_q2d(stream->time_base) * pkt->dts * 1000);
    long duration = static_cast<long>(av_q2d(stream->time_base) * pkt->duration * 1000);

    if (sub.format != 1)
    {
        LOG(VB_VBI, LOG_INFO, QString("Time %1, Not a text subtitle").arg(start));
        return -1;
    }

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    const auto * codec = QTextCodec::codecForName("utf-8");
#else
    auto toUtf16 = QStringDecoder(QStringDecoder::Utf8);
#endif
    for (uint i = 0 ; i < sub.num_rects; i++)
    {
        text_subtitle_t newsub(start, start + duration);
        QString text;
        if (sub.rects[i]->type == SUBTITLE_TEXT)
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            text = codec->toUnicode(sub.rects[i]->text, strlen(sub.rects[i]->text));
#else
            text = toUtf16.decode(sub.rects[i]->text);
#endif
            newsub.m_textLines.push_back(text);
        }
        else if (sub.rects[i]->type == SUBTITLE_ASS)
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            text = codec->toUnicode(sub.rects[i]->ass, strlen(sub.rects[i]->ass));
#else
            text = toUtf16.decode(sub.rects[i]->ass);
#endif
            static const QRegularExpression continuation(R"(\\+n +)",
                                                  QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression endOfLine(R"(\\+[Nn])",
                                               QRegularExpression::CaseInsensitiveOption);
            // ASS "event" line. Comma seperated. Last field is the text.
            text = text.section(QChar(','),-1);
            // collapse continuation lines
            text = text.remove(continuation);
            // split into multiple lines
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
            QStringList lines = text.split(endOfLine, QString::KeepEmptyParts);
#else
            QStringList lines = text.split(endOfLine, Qt::KeepEmptyParts);
#endif
            newsub.m_textLines.append(lines);
        }
        target.AddSubtitle(newsub);
    }

    avsubtitle_free(&sub);
    return ret;
}

void TextSubtitleParser::LoadSubtitles(const QString &fileName,
                                       TextSubtitles &target,
                                       bool inBackground)
{
    std::string errbuf;

    if (inBackground)
    {
        if (!SubtitleLoadHelper::IsLoading(&target))
        {
            MThreadPool::globalInstance()->
                start(new SubtitleLoadHelper(fileName, &target),
                      "SubtitleLoadHelper");
        }
        return;
    }
    local_buffer_t sub_data {};
    RemoteFileWrapper rfile(fileName/*, false, false, 0*/);

    LOG(VB_VBI, LOG_INFO,
        QString("Preparing to load subtitle file %1").arg(fileName));
    if (!rfile.isOpen())
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to load subtitle file %1").arg(fileName));
        return;
    }
    target.SetHasSubtitles(true);
    target.SetFilename(fileName);

    // Only reload if rfile.GetFileSize() has changed.
    off_t new_len = rfile.GetFileSize();
    if (target.GetByteCount() == new_len)
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Filesize unchanged (%1), not reloading subs (%2)")
            .arg(new_len).arg(fileName));
        target.SetLastLoaded();
        return;
    }
    LOG(VB_VBI, LOG_INFO,
        QString("Preparing to read %1 subtitle bytes from %2")
        .arg(new_len).arg(fileName));
    target.SetByteCount(new_len);
    sub_data.rbuffer_len = new_len;
    sub_data.rbuffer_text = new char[sub_data.rbuffer_len + 1];
    sub_data.rbuffer_cur = 0;

    // Slurp the entire file into a buffer.
    int numread = rfile.Read(sub_data.rbuffer_text, sub_data.rbuffer_len);
    LOG(VB_VBI, LOG_INFO,
        QString("Finished reading %1 subtitle bytes (requested %2)")
        .arg(numread).arg(new_len));

    // Create a format context and tie it to the file buffer.
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == nullptr) {
        LOG(VB_VBI, LOG_INFO, "Couldn't allocate format context");
        return;
    }
    auto *avio_ctx_buffer = (uint8_t*)av_malloc(IO_BUFFER_SIZE);
    if (avio_ctx_buffer == nullptr)
    {
        LOG(VB_VBI, LOG_INFO, "Couldn't allocate mamory for avio context");
        avformat_free_context(fmt_ctx);
        return;
    }
    fmt_ctx->pb = avio_alloc_context(avio_ctx_buffer, IO_BUFFER_SIZE,
                                     0, &sub_data,
                                     &read_packet, nullptr, &seek_packet);
    if(int ret = avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr); ret < 0) {
        LOG(VB_VBI, LOG_INFO, QString("Couldn't open input context %1")
            .arg(av_make_error_stdstring(errbuf,ret)));
        // FFmpeg frees context on error.
        return;
    }

    // Find the subtitle stream and its context.
    const AVCodec *codec {nullptr};
    int stream_num = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_SUBTITLE, -1, -1, &codec, 0);
    if (stream_num < 0) {
        LOG(VB_VBI, LOG_INFO, QString("Couldn't find subtitle stream. %1")
            .arg(av_make_error_stdstring(errbuf,stream_num)));
        avformat_free_context(fmt_ctx);
        return;
    }
    AVStream *stream = fmt_ctx->streams[stream_num];
    if (stream == nullptr) {
        LOG(VB_VBI, LOG_INFO, QString("Stream %1 is null").arg(stream_num));
        avformat_free_context(fmt_ctx);
        return;
    }

    // Create a decoder for this subtitle stream context.
    AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        LOG(VB_VBI, LOG_INFO, QString("Couldn't allocate decoder context"));
        avformat_free_context(fmt_ctx);
        return;
    }
    if (avcodec_open2(dec_ctx, codec, nullptr) < 0) {
        LOG(VB_VBI, LOG_INFO, QString("Couldn't open decoder context"));
        avcodec_free_context(&dec_ctx);
        avformat_free_context(fmt_ctx);
        return;
    }

    /* decode until eof */
    AVPacket *pkt = av_packet_alloc();
    av_new_packet(pkt, 4096);
    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        int bytes {0};
        while ((bytes = decode(target, dec_ctx, stream, pkt)) >= 0)
        {
            pkt->data += bytes;
            pkt->size -= bytes;
        }

        // reset buffer for next packet
        pkt->data = pkt->buf->data;
        pkt->size = pkt->buf->size;
    }

    /* flush the decoder */
    pkt->data = nullptr;
    pkt->size = 0;
    while (decode(target, dec_ctx, stream, pkt) >= 0)
    {
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Loaded %1 %2 subtitles from '%3'")
        .arg(target.GetSubtitleCount()).arg(codec->long_name, fileName));
    target.SetLastLoaded();

    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    avformat_free_context(fmt_ctx);
}
