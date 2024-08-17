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
#include <QRunnable>
#include <QFile>
#include <QDataStream>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#elif QT_VERSION < QT_VERSION_CHECK(6,3,0)
#include <QStringConverter>
#endif

// MythTV
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"
#include "libmythtv/io/mythmediabuffer.h"
#include "libmyth/mythaverror.h"

#include "captions/textsubtitleparser.h"
#include "captions/subtitlereader.h"

static constexpr uint32_t IO_BUFFER_SIZE { 32768 };

// This background thread helper class is adapted from the
// RebuildSaver class in mythcommflagplayer.cpp.
class SubtitleLoadHelper : public QRunnable
{
  public:
    SubtitleLoadHelper(TextSubtitleParser *parent,
                       TextSubtitles *target)

        : m_parent(parent), m_target(target)
    {
        QMutexLocker locker(&s_lock);
        ++s_loading[m_target];
    }

    void run(void) override // QRunnable
    {
        m_parent->LoadSubtitles(false);

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
    TextSubtitleParser *m_parent { nullptr };
    TextSubtitles      *m_target { nullptr };

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

TextSubtitles::~TextSubtitles()
{
    SubtitleLoadHelper::Wait(this);
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

TextSubtitleParser::TextSubtitleParser(SubtitleReader *parent, QString fileName, TextSubtitles *target)
    : m_parent(parent),
      m_target(target),
      m_fileName(std::move(fileName)),
      m_pkt(av_packet_alloc())
{
}

TextSubtitleParser::~TextSubtitleParser()
{
    avcodec_free_context(&m_decCtx);
    avformat_free_context(m_fmtCtx);
    av_packet_free(&m_pkt);
    m_stream = nullptr;
    delete m_loadHelper;
}

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

/// \brief Read the next subtitle in the AV stream.
///
/// av_read_frame guarantees that pkt->pts, pkt->dts and pkt->duration
/// are always set to correct values in AVStream.time_base units (and
/// guessed if the format cannot provide them). pkt->pts can be
/// AV_NOPTS_VALUE if the video format has B-frames, so it is better to
/// rely on pkt->dts if you do not decompress the payload.
int TextSubtitleParser::ReadNextSubtitle(void)
{

    int ret = av_read_frame(m_fmtCtx, m_pkt);
    if (ret < 0)
        return ret;

    AVSubtitle sub {};
    int got_sub_ptr {0};
    ret = avcodec_decode_subtitle2(m_decCtx, &sub, &got_sub_ptr, m_pkt);
    if (ret < 0)
        return ret;
    if (!got_sub_ptr)
        return -1;

    sub.start_display_time = av_q2d(m_stream->time_base) * m_pkt->dts * 1000;
    sub.end_display_time = av_q2d(m_stream->time_base) * (m_pkt->dts + m_pkt->duration) * 1000;

    m_parent->AddAVSubtitle(sub, m_decCtx->codec_id == AV_CODEC_ID_XSUB, false, false, true);
    return ret;
}

void TextSubtitleParser::LoadSubtitles(bool inBackground)
{
    std::string errbuf;

    if (inBackground)
    {
        if (!SubtitleLoadHelper::IsLoading(m_target))
        {
            m_loadHelper = new SubtitleLoadHelper(this, m_target);
            MThreadPool::globalInstance()->
                start(m_loadHelper, "SubtitleLoadHelper");
        }
        return;
    }

    // External subtitles are now presented as AV Subtitles.
    m_parent->EnableAVSubtitles(false);
    m_parent->EnableTextSubtitles(true);
    m_parent->EnableRawTextSubtitles(false);

    local_buffer_t sub_data {};
    RemoteFileWrapper rfile(m_fileName/*, false, false, 0*/);

    LOG(VB_VBI, LOG_INFO,
        QString("Preparing to load subtitle file %1").arg(m_fileName));
    if (!rfile.isOpen())
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to load subtitle file %1").arg(m_fileName));
        return;
    }
    m_target->SetHasSubtitles(true);
    m_target->SetFilename(m_fileName);

    // Only reload if rfile.GetFileSize() has changed.
    // RemoteFile::GetFileSize can return -1 on error.
    off_t new_len = rfile.GetFileSize();
    if (new_len < 0)
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to get file size for %1").arg(m_fileName));
        return;
    }

    if (m_target->GetByteCount() == new_len)
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Filesize unchanged (%1), not reloading subs (%2)")
            .arg(new_len).arg(m_fileName));
        m_target->SetLastLoaded();
        return;
    }
    LOG(VB_VBI, LOG_INFO,
        QString("Preparing to read %1 subtitle bytes from %2")
        .arg(new_len).arg(m_fileName));
    m_target->SetByteCount(new_len);
    sub_data.rbuffer_len = new_len;
    sub_data.rbuffer_text = new char[sub_data.rbuffer_len + 1];
    sub_data.rbuffer_cur = 0;

    // Slurp the entire file into a buffer.
    int numread = rfile.Read(sub_data.rbuffer_text, sub_data.rbuffer_len);
    LOG(VB_VBI, LOG_INFO,
        QString("Finished reading %1 subtitle bytes (requested %2)")
        .arg(numread).arg(new_len));
    bool isUtf8 {false};
    auto qba = QByteArray::fromRawData(sub_data.rbuffer_text,
                                       sub_data.rbuffer_len);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QTextCodec *textCodec = QTextCodec::codecForUtfText(qba, nullptr);
    isUtf8 = (textCodec != nullptr);
#elif QT_VERSION < QT_VERSION_CHECK(6,3,0)
    auto qba_encoding = QStringConverter::encodingForData(qba);
    isUtf8 = qba_encoding.has_value()
        && (qba_encoding.value() == QStringConverter::Utf8);
#else
    isUtf8 = qba.isValidUtf8();
#endif

    // Create a format context and tie it to the file buffer.
    m_fmtCtx = avformat_alloc_context();
    if (m_fmtCtx == nullptr) {
        LOG(VB_VBI, LOG_INFO, "Couldn't allocate format context");
        return;
    }
    auto *avio_ctx_buffer = (uint8_t*)av_malloc(IO_BUFFER_SIZE);
    if (avio_ctx_buffer == nullptr)
    {
        LOG(VB_VBI, LOG_INFO, "Couldn't allocate memory for avio context");
        avformat_free_context(m_fmtCtx);
        return;
    }
    m_fmtCtx->pb = avio_alloc_context(avio_ctx_buffer, IO_BUFFER_SIZE,
                                     0, &sub_data,
                                     &read_packet, nullptr, &seek_packet);
    if (int ret = avformat_open_input(&m_fmtCtx, nullptr, nullptr, nullptr); ret < 0) {
        LOG(VB_VBI, LOG_INFO, QString("Couldn't open input context %1")
            .arg(av_make_error_stdstring(errbuf,ret)));
        // FFmpeg frees context on error.
        return;
    }

    // Find the subtitle stream and its context.
    QString encoding {"utf-8"};
    if (!m_decCtx)
    {
        const AVCodec *codec {nullptr};
        int stream_num = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_SUBTITLE, -1, -1, &codec, 0);
        if (stream_num < 0) {
            LOG(VB_VBI, LOG_INFO, QString("Couldn't find subtitle stream. %1")
                .arg(av_make_error_stdstring(errbuf,stream_num)));
            avformat_free_context(m_fmtCtx);
            return;
        }
        m_stream = m_fmtCtx->streams[stream_num];
        if (m_stream == nullptr) {
            LOG(VB_VBI, LOG_INFO, QString("Stream %1 is null").arg(stream_num));
            avformat_free_context(m_fmtCtx);
            return;
        }

        // Create a decoder for this subtitle stream context.
        m_decCtx = avcodec_alloc_context3(codec);
        if (!m_decCtx) {
            LOG(VB_VBI, LOG_INFO, QString("Couldn't allocate decoder context"));
            avformat_free_context(m_fmtCtx);
            return;
        }

        // Ask FFmpeg to convert subtitles to utf-8.
        AVDictionary *dict = nullptr;
        if (!isUtf8)
        {
            encoding = gCoreContext->GetSetting("SubtitleCodec", "utf-8");
            if (encoding != "utf-8")
            {
                LOG(VB_VBI, LOG_INFO,
                    QString("Converting from %1 to utf-8.").arg(encoding));
                av_dict_set(&dict, "sub_charenc", qPrintable(encoding), 0);
            }
        }
        if (avcodec_open2(m_decCtx, codec, &dict) < 0) {
            LOG(VB_VBI, LOG_INFO,
                QString("Couldn't open decoder context for encoding %1").arg(encoding));
            avcodec_free_context(&m_decCtx);
            avformat_free_context(m_fmtCtx);
            return;
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Loaded %2 '%3' subtitles from %4")
        .arg(encoding, m_decCtx->codec->long_name, m_fileName));
    m_target->SetLastLoaded();
}

QByteArray TextSubtitleParser::GetSubHeader()
{
    if (nullptr == m_decCtx)
        return {};
    return { reinterpret_cast<char*>(m_decCtx->subtitle_header),
             m_decCtx->subtitle_header_size };
}

void TextSubtitleParser::SeekFrame(int64_t ts, int flags)
{
    if (av_seek_frame(m_fmtCtx, -1, ts, flags) < 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("TextSubtitleParser av_seek_frame(fmtCtx, -1, %1, %2) -- error")
            .arg(ts).arg(flags));
    }
}
