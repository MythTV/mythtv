// -*- Mode: c++ -*-
/** TextSubtitles
 *  Copyright (c) 2006 by Pekka J��skel�inen
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
#include <QTextCodec>

// MythTV
#include "mythcorecontext.h"
#include "ringbuffer.h"
#include "textsubtitleparser.h"
#include "xine_demux_sputext.h"

static bool operator<(const text_subtitle_t& left,
                      const text_subtitle_t& right)
{
    return left.start < right.start;
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
QStringList TextSubtitles::GetSubtitles(uint64_t timecode) const
{
    QStringList list;
    if (m_subtitles.empty())
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
        // at the end of video, the blank subtitle should last until
        // forever
        endCode = startCode + INT_MAX;
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
    m_lock.lock();
    m_subtitles.push_back(newSub);
    m_lock.unlock();
}

void TextSubtitles::Clear(void)
{
    m_lock.lock();
    m_subtitles.clear();
    m_lock.unlock();
}

bool TextSubtitleParser::LoadSubtitles(QString fileName, TextSubtitles &target)
{
    demux_sputext_t sub_data;
    sub_data.rbuffer = RingBuffer::Create(fileName, 0, false);

    if (!sub_data.rbuffer)
        return false;

    subtitle_t *loaded_subs = sub_read_file(&sub_data);
    if (!loaded_subs)
    {
        delete sub_data.rbuffer;
        return false;
    }

    target.SetFrameBasedTiming(!sub_data.uses_time);

    QTextCodec *textCodec = NULL;
    QString codec = gCoreContext->GetSetting("SubtitleCodec", "");
    if (!codec.isEmpty())
        textCodec = QTextCodec::codecForName(codec.toLatin1());
    if (!textCodec)
        textCodec = QTextCodec::codecForName("utf-8");
    if (!textCodec)
    {
        delete sub_data.rbuffer;
        return false;
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
    delete sub_data.rbuffer;

    return true;
}
