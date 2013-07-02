// -*- Mode: c++ -*-
/** TextSubtitles
 *  Copyright (c) 2006 by Pekka J‰‰skel‰inen
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// ANSI C
#include <cstdio>
#include <cstring>
#include <climits>

// C++
#include <algorithm>
using std::lower_bound;

// Qt
#include <QRunnable>
#include <QTextCodec>

// MythTV
#include "mythcorecontext.h"
#include "remotefile.h"
#include "textsubtitleparser.h"
#include "xine_demux_sputext.h"
#include "mythlogging.h"
#include "mthreadpool.h"

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

    virtual void run(void)
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
        return s_loading[target];
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

    static QMutex                    s_lock;
    static QWaitCondition            s_wait;
    static QMap<TextSubtitles*,uint> s_loading;
};
QMutex                    SubtitleLoadHelper::s_lock;
QWaitCondition            SubtitleLoadHelper::s_wait;
QMap<TextSubtitles*,uint> SubtitleLoadHelper::s_loading;

static bool operator<(const text_subtitle_t& left,
                      const text_subtitle_t& right)
{
    return left.start < right.start;
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
    return (timecode < m_lastReturnedSubtitle.start ||
            timecode > m_lastReturnedSubtitle.end);
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
    QStringList list;
    if (!m_isInProgress && m_subtitles.empty())
        return list;

    text_subtitle_t searchTarget(timecode, timecode);

    TextSubtitleList::const_iterator nextSubPos =
        lower_bound(m_subtitles.begin(), m_subtitles.end(), searchTarget);

    uint64_t startCode = 0, endCode = 0;
    if (nextSubPos != m_subtitles.begin())
    {
        TextSubtitleList::const_iterator currentSubPos = nextSubPos;
        --currentSubPos;

        const text_subtitle_t &sub = *currentSubPos;
        if (sub.start <= timecode && sub.end >= timecode)
        {
            // found a sub to display
            m_lastReturnedSubtitle = sub;
            QStringList tmp = m_lastReturnedSubtitle.textLines;
            tmp.detach();
            return tmp;
        }

        // the subtitle time span has ended, let's display a blank sub
        startCode = sub.end + 1;
    }

    if (nextSubPos == m_subtitles.end())
    {
        if (m_isInProgress)
        {
            const int maxReloadInterval = 1000; // ms
            if (IsFrameBasedTiming())
                // Assume conservative 24fps
                endCode = startCode + maxReloadInterval / 24;
            else
                endCode = startCode + maxReloadInterval;
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
        endCode = (*nextSubPos).start - 1;
    }

    // we are in a position in which there are no subtitles to display,
    // return an empty subtitle and create a dummy empty subtitle for this
    // time span so SubtitleChanged() functions also in this case
    text_subtitle_t blankSub(startCode, endCode);
    m_lastReturnedSubtitle = blankSub;

    return list;
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
    QMutexLocker locker(&m_lock);
    m_lastLoaded = QDateTime::currentDateTimeUtc();
}

void TextSubtitleParser::LoadSubtitles(const QString &fileName,
                                       TextSubtitles &target,
                                       bool inBackground)
{
    if (inBackground)
    {
        if (!SubtitleLoadHelper::IsLoading(&target))
            MThreadPool::globalInstance()->
                start(new SubtitleLoadHelper(fileName, &target),
                      "SubtitleLoadHelper");
        return;
    }
    demux_sputext_t sub_data;
    RemoteFile rfile(fileName, false, false, 0);

    LOG(VB_VBI, LOG_INFO,
        QString("Preparing to load subtitle file (%1)").arg(fileName));
    if (!rfile.isOpen())
    {
        LOG(VB_VBI, LOG_INFO,
            QString("Failed to load subtitle file (%1)").arg(fileName));
        return;
    }
    target.SetHasSubtitles(true);
    target.SetFilename(fileName);

    // Only reload if rfile.GetRealFileSize() has changed.
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
    sub_data.errs = 0;
    int numread = rfile.Read(sub_data.rbuffer_text, sub_data.rbuffer_len);
    LOG(VB_VBI, LOG_INFO,
        QString("Finished reading %1 subtitle bytes (requested %2)")
        .arg(numread).arg(new_len));
    subtitle_t *loaded_subs = sub_read_file(&sub_data);
    if (!loaded_subs)
    {
        delete sub_data.rbuffer_text;
        return;
    }

    target.SetFrameBasedTiming(!sub_data.uses_time);
    target.Clear();

    QTextCodec *textCodec = NULL;
    QString codec = gCoreContext->GetSetting("SubtitleCodec", "");
    if (!codec.isEmpty())
        textCodec = QTextCodec::codecForName(codec.toLatin1());
    if (!textCodec)
        textCodec = QTextCodec::codecForName("utf-8");
    if (!textCodec)
    {
        delete sub_data.rbuffer_text;
        return;
    }

    QTextDecoder *dec = textCodec->makeDecoder();

    // convert the subtitles to our own format and free the original structures
    for (int sub_i = 0; sub_i < sub_data.num; ++sub_i)
    {
        const subtitle_t *sub = &loaded_subs[sub_i];
        text_subtitle_t newsub(sub->start, sub->end);

        if (!target.IsFrameBasedTiming())
        {
            newsub.start *= 10; // convert from csec to msec
            newsub.end *= 10;
        }

        for (int line = 0; line < sub->lines; ++line)
        {
            const char *subLine = sub->text[line];
            QString str = dec->toUnicode(subLine, strlen(subLine));
            newsub.textLines.push_back(str);

            free(sub->text[line]);
        }
        target.AddSubtitle(newsub);
    }

    delete dec;
    // textCodec object is managed by Qt, do not delete...

    free(loaded_subs);
    delete sub_data.rbuffer_text;

    target.SetLastLoaded();
}
