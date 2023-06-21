// -*- Mode: c++ -*-
/** TextSubtitles
 *  Copyright (c) 2006 by Pekka Jääskeläinen
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef TEXT_SUBTITLE_PARSER_H
#define TEXT_SUBTITLE_PARSER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// C++ headers
#include <cstdint>
#include <vector>

// Qt headers
#include <QObject>
#include <QMutexLocker>
#include <QStringList>
#include <QDateTime>

class text_subtitle_t
{
  public:
    text_subtitle_t(long start_, long end_) : m_start(start_), m_end(end_) {}
    text_subtitle_t() = default;
    text_subtitle_t(const text_subtitle_t&) = default;
    text_subtitle_t& operator= (const text_subtitle_t&) = default;

  public:
    uint64_t    m_start {0};  ///< Starting time in msec or starting frame
    uint64_t    m_end   {0};  ///< Ending time in msec or ending frame
    QStringList m_textLines;
};

using TextSubtitleList = std::vector<text_subtitle_t>;

class TextSubtitles : public QObject
{
    Q_OBJECT

  signals:
    void TextSubtitlesUpdated();

  public:
    ~TextSubtitles() override;

    void SetFilename(const QString &fileName) {
        QMutexLocker locker(&m_lock);
        m_fileName = fileName;
    }

    void SetLastLoaded(void);
    void SetByteCount(off_t count) {
        QMutexLocker locker(&m_lock);
        m_byteCount = count;
    }
    off_t GetByteCount(void) const { return m_byteCount; }
    void SetInProgress(bool isInProgress) {
        QMutexLocker locker(&m_lock);
        m_isInProgress = isInProgress;
    }
    void SetHasSubtitles(bool hasSubs) { m_hasSubtitles = hasSubs; }
    bool GetHasSubtitles() const { return m_hasSubtitles; }

    void Lock(void)   { m_lock.lock();   }
    void Unlock(void) { m_lock.unlock(); }

  private:
    QString                   m_fileName;
    QDateTime                 m_lastLoaded;
    off_t                     m_byteCount        {0};
    // Note: m_isInProgress is overly conservative because it doesn't
    // change from true to false after a recording completes.
    bool                      m_isInProgress     {false};
    bool                      m_hasSubtitles     {false};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex                    m_lock             {QMutex::Recursive};
#else
    QRecursiveMutex           m_lock;
#endif
};

class SubtitleReader;
class SubtitleLoadHelper;

class TextSubtitleParser
{
  public:
    TextSubtitleParser(SubtitleReader *parent, QString fileName, TextSubtitles *target)
        : m_parent(parent), m_target(target), m_fileName(std::move(fileName)) {};
    ~TextSubtitleParser();
    void LoadSubtitles(bool inBackground);
    int  decode(AVPacket *pkt);
    QByteArray GetSubHeader();

  private:
    static int     read_packet(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek_packet(void *opaque, int64_t offset, int whence);

    SubtitleReader     *m_parent     {nullptr};
    SubtitleLoadHelper *m_loadHelper {nullptr};
    TextSubtitles      *m_target     {nullptr};
    QString             m_fileName;

    AVCodecContext     *m_decCtx     {nullptr};
    AVStream           *m_stream     {nullptr};

    uint32_t            m_count      {0};
};

#endif
