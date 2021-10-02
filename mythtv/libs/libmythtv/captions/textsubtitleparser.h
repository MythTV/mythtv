// -*- Mode: c++ -*-
/** TextSubtitles
 *  Copyright (c) 2006 by Pekka Jääskeläinen
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef TEXT_SUBTITLE_PARSER_H
#define TEXT_SUBTITLE_PARSER_H

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
    TextSubtitles()
    {
        m_lastReturnedSubtitle.m_start = 0;
        m_lastReturnedSubtitle.m_end   = 0;
    }

    ~TextSubtitles() override;

    bool HasSubtitleChanged(uint64_t timecode) const;
    QStringList GetSubtitles(uint64_t timecode);

    /** \fn TextSubtitles::IsFrameBasedTiming(void) const
     *  \brief Returns true in case the subtitle timing data is frame-based.
     *
     *  If the timing is frame-based, the client should use frame counts as
     *  timecodes for the HasSubtitleChanged() and GetSubtitles() methods,
     *  otherwise the timecode is milliseconds from the video start.
     */
    bool IsFrameBasedTiming(void) const
        { return m_frameBasedTiming; }

    void SetFrameBasedTiming(bool frameBasedTiming) {
        QMutexLocker locker(&m_lock);
        m_frameBasedTiming = frameBasedTiming;
    }

    void SetFilename(const QString &fileName) {
        QMutexLocker locker(&m_lock);
        m_fileName = fileName;
    }

    void AddSubtitle(const text_subtitle_t& newSub);
    void Clear(void);
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

    // Returns -1 if there is no subtitle file.
    int GetSubtitleCount(void) const
        { return m_hasSubtitles ? m_subtitles.size() : -1; }

    void Lock(void)   { m_lock.lock();   }
    void Unlock(void) { m_lock.unlock(); }

  private:
    TextSubtitleList          m_subtitles;
    mutable text_subtitle_t   m_lastReturnedSubtitle;
    bool                      m_frameBasedTiming {false};
    QString                   m_fileName;
    QDateTime                 m_lastLoaded;
    off_t                     m_byteCount        {0};
    // Note: m_isInProgress is overly conservative because it doesn't
    // change from true to false after a recording completes.
    bool                      m_isInProgress     {false};
    // It's possible to have zero subtitles at the start of playback
    // because none have yet been written for an in-progress
    // recording, so use m_hasSubtitles instead of m_subtitles.size().
    bool                      m_hasSubtitles     {false};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex                    m_lock             {QMutex::Recursive};
#else
    QRecursiveMutex           m_lock;
#endif
};

class TextSubtitleParser
{
  public:
    static void LoadSubtitles(const QString &fileName, TextSubtitles &target,
                              bool inBackground);
};

#endif
