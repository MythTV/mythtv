#include <algorithm>

#include "captions/cc608reader.h"
#include "mythplayer.h"
#include "recorders/vbitext/vbi.h"

CC608Reader::CC608Reader(MythPlayer *parent)
  : m_parent(parent),
    m_maxTextSize(8 * (sizeof(teletextsubtitle) + VT_WIDTH))
{
    for (int i = 0; i < MAXTBUFFER; i++)
        m_inputBuffers[i].buffer = new unsigned char[m_maxTextSize + 1];
}

CC608Reader::~CC608Reader()
{
    ClearBuffers(true, true);
    for (int i = 0; i < MAXTBUFFER; i++)
    {
        if (m_inputBuffers[i].buffer)
        {
            delete [] m_inputBuffers[i].buffer;
            m_inputBuffers[i].buffer = nullptr;
        }
    }
}

void CC608Reader::FlushTxtBuffers(void)
{
    QMutexLocker locker(&m_inputBufLock);
    m_readPosition = m_writePosition;
}

CC608Buffer *CC608Reader::GetOutputText(bool &changed)
{
    bool last_changed = true;
    while (last_changed)
    {
        last_changed = false;
        int streamIdx = -1;
        CC608Buffer *tmp = GetOutputText(last_changed, streamIdx);
        if (last_changed && (((streamIdx << 4) & CC_MODE_MASK) == m_ccMode))
        {
            changed = true;
            return tmp;
        }
    }

    return nullptr;
}

CC608Buffer *CC608Reader::GetOutputText(bool &changed, int &streamIdx)
{
    streamIdx = -1;

    if (!m_enabled)
        return nullptr;

    if (!m_parent)
    {
        if (NumInputBuffers())
        {
            streamIdx = Update(m_inputBuffers[m_writePosition].buffer);
            changed = true;

            QMutexLocker locker(&m_inputBufLock);
            if (m_writePosition != m_readPosition)
                m_writePosition = (m_writePosition + 1) % MAXTBUFFER;
        }

        if (streamIdx >= 0)
        {
            m_state[streamIdx].m_changed = false;
            return &m_state[streamIdx].m_output;
        }
        return &m_state[MAXOUTBUFFERS - 1].m_output;
    }

    MythVideoFrame *last = nullptr;
    if (m_parent->GetVideoOutput())
        last = m_parent->GetVideoOutput()->GetLastShownFrame();

    if (NumInputBuffers() && (m_inputBuffers[m_writePosition].timecode > 0ms) &&
       (last && m_inputBuffers[m_writePosition].timecode <= last->m_timecode))
    {
        if (m_inputBuffers[m_writePosition].type == 'T')
        {
            streamIdx = MAXOUTBUFFERS - 1;

            // display full page of teletext
            //
            // all formatting is always defined in the page itself,
            // if scrolling is needed for live closed captions this
            // is handled by the broadcaster:
            // the pages are then very often transmitted (sometimes as often as
            // every 2 frames) with small differences between them
            unsigned char *inpos = m_inputBuffers[m_writePosition].buffer;
            int pagenr = 0;
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);

            if (pagenr == (m_ccPageNum<<16))
            {
                // show teletext subtitles
                ClearBuffers(false, true, streamIdx);
                (*inpos)++;
                while (*inpos)
                {
                    struct teletextsubtitle st {};
                    memcpy(&st, inpos, sizeof(st));
                    inpos += sizeof(st);

#if 0
                    // With the removal of support for cc608 teletext,
                    // we still want to skip over any teletext packets
                    // that might inadvertently be present.
                    CC608Text *cc = new CC608Text(
                        QString((const char*) inpos), st.row, st.col);

                    m_state[streamIdx].m_output.lock.lock();
                    m_state[streamIdx].m_output.buffers.push_back(cc);
                    m_state[streamIdx].m_output.lock.unlock();
#endif // 0

                    inpos += st.len;
                }
                //changed = true;
            }
        }
        else if (m_inputBuffers[m_writePosition].type == 'C')
        {
            streamIdx = Update(m_inputBuffers[m_writePosition].buffer);
            changed = true;
        }

        QMutexLocker locker(&m_inputBufLock);
        if (m_writePosition != m_readPosition)
            m_writePosition = (m_writePosition + 1) % MAXTBUFFER;
    }

    if (streamIdx >= 0)
    {
        m_state[streamIdx].m_changed = false;
        return &m_state[streamIdx].m_output;
    }
    return &m_state[MAXOUTBUFFERS - 1].m_output;
}

void CC608Reader::SetMode(int mode)
{
    // TODO why was the clearing code removed?
    //int oldmode = m_ccMode;
    if (mode <= 2)
        m_ccMode = (mode == 1) ? CC_CC1 : CC_CC2;
    else
        m_ccMode = (mode == 3) ? CC_CC3 : CC_CC4;
    //if (oldmode != m_ccMode)
    //    ClearBuffers(true, true);
}

int CC608Reader::Update(unsigned char *inpos)
{
    struct ccsubtitle subtitle {};

    memcpy(&subtitle, inpos, sizeof(subtitle));
    inpos += sizeof(ccsubtitle);

    const int streamIdx = (subtitle.resumetext & CC_MODE_MASK) >> 4;

    if (subtitle.row == 0)
        subtitle.row = 1;

    if (subtitle.clr)
    {
#if 0
        LOG(VB_VBI, LOG_DEBUG, "erase displayed memory");
#endif
        ClearBuffers(false, true, streamIdx);
        if (!subtitle.len)
            return streamIdx;
    }

//    if (subtitle.len || !subtitle.clr)
    {
        unsigned char *end = inpos + subtitle.len;
        int row = 0;
        int linecont = (subtitle.resumetext & CC_LINE_CONT);

        auto *ccbuf = new std::vector<CC608Text*>;
        CC608Text *tmpcc = nullptr;
        int replace = linecont;
        int scroll = 0;
        bool scroll_prsv = false;
        int scroll_yoff = 0;
        int scroll_ymax = 15;

        do
        {
            if (linecont)
            {
                // append to last line; needs to be redrawn
                replace = 1;
                // backspace into existing line if needed
                int bscnt = 0;
                while ((inpos < end) && *inpos != 0 && (char)*inpos == '\b')
                {
                    bscnt++;
                    inpos++;
                }
                if (bscnt)
                {
                    m_state[streamIdx].m_outputText.remove(
                        m_state[streamIdx].m_outputText.length() - bscnt,
                        bscnt);
                }
            }
            else
            {
                // new line:  count spaces to calculate column position
                row++;
                m_state[streamIdx].m_outputCol = 0;
                m_state[streamIdx].m_outputText = "";
                while ((inpos < end) && *inpos != 0 && (char)*inpos == ' ')
                {
                    inpos++;
                    m_state[streamIdx].m_outputCol++;
                }
            }

            m_state[streamIdx].m_outputRow = subtitle.row;
            unsigned char *cur = inpos;

            //null terminate at EOL
            while (cur < end && *cur != '\n' && *cur != 0)
                cur++;
            *cur = 0;

            if (*inpos != 0 || linecont)
            {
                if (linecont)
                {
                    m_state[streamIdx].m_outputText +=
                        QString::fromUtf8((const char *)inpos, -1);
                }
                else
                {
                    m_state[streamIdx].m_outputText =
                        QString::fromUtf8((const char *)inpos, -1);
                }
                tmpcc = new CC608Text(
                    m_state[streamIdx].m_outputText,
                    m_state[streamIdx].m_outputCol,
                    m_state[streamIdx].m_outputRow);
                ccbuf->push_back(tmpcc);
#if 0
                if (ccbuf->size() > 4)
                    LOG(VB_VBI, LOG_DEBUG, QString("CC overflow: %1 %2 %3")
                        .arg(m_state[streamIdx].m_outputCol)
                        .arg(m_state[streamIdx].m_outputRow)
                        .arg(m_state[streamIdx].m_outputText));
#endif
            }
            subtitle.row++;
            inpos = cur + 1;
            linecont = 0;
        } while (inpos < end);

        // adjust row position
        if (subtitle.resumetext & CC_TXT_MASK)
        {
            // TXT mode
            // - can use entire 15 rows
            // - scroll up when reaching bottom
            if (m_state[streamIdx].m_outputRow > 15)
            {
                if (row)
                    scroll = m_state[streamIdx].m_outputRow - 15;
                if (tmpcc)
                    tmpcc->m_y = 15;
            }
        }
        else if (subtitle.rowcount == 0 || row > 1)
        {
            // multi-line text
            // - fix display of old (badly-encoded) files
            if (m_state[streamIdx].m_outputRow > 15)
            {
                for (auto & ccp : *ccbuf)
                {
                    tmpcc = ccp;
                    tmpcc->m_y -= (m_state[streamIdx].m_outputRow - 15);
                }
            }
        }
        else
        {
            // scrolling text
            // - scroll up previous lines if adding new line
            // - if caption is at bottom, row address is for last
            // row
            // - if caption is at top, row address is for first row (?)
            subtitle.rowcount = std::min<int>(subtitle.rowcount, 4);
            if (m_state[streamIdx].m_outputRow < subtitle.rowcount)
            {
                m_state[streamIdx].m_outputRow = subtitle.rowcount;
                if (tmpcc)
                    tmpcc->m_y = m_state[streamIdx].m_outputRow;
            }
            if (row)
            {
                scroll = row;
                scroll_prsv = true;
                scroll_yoff =
                    m_state[streamIdx].m_outputRow - subtitle.rowcount;
                scroll_ymax = m_state[streamIdx].m_outputRow;
            }
        }

        Update608Text(ccbuf, replace, scroll,
                      scroll_prsv, scroll_yoff, scroll_ymax, streamIdx);
        delete ccbuf;
    }

    return streamIdx;
}

void CC608Reader::TranscodeWriteText(CC608WriteFn func, void * ptr)
{
    QMutexLocker locker(&m_inputBufLock);
    while (NumInputBuffers(false))
    {
        locker.unlock();
        int pagenr = 0;
        unsigned char *inpos = m_inputBuffers[m_readPosition].buffer;
        if (m_inputBuffers[m_readPosition].type == 'T')
        {
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);
            m_inputBuffers[m_readPosition].len -= sizeof(int);
        }
        func(ptr, inpos, m_inputBuffers[m_readPosition].len,
             m_inputBuffers[m_readPosition].timecode, pagenr);

        locker.relock();
        m_readPosition = (m_readPosition + 1) % MAXTBUFFER;
    }
}

void CC608Reader::Update608Text(
    std::vector<CC608Text*> *ccbuf, int replace, int scroll, bool scroll_prsv,
    int scroll_yoff, int scroll_ymax, int streamIdx)
// ccbuf      :  new text
// replace    :  replace last lines
// scroll     :  scroll amount
// scroll_prsv:  preserve last lines and move into scroll window
// scroll_yoff:  yoff < scroll window <= ymax
// scroll_ymax:
{
#if 0
    LOG(VB_VBI, LOG_DEBUG, QString("%1:%2 ").arg(__FUNCTION__).arg(__LINE__) +
        QString("replace:%1 ").arg(replace) +
        QString("scroll:%1 ").arg(scroll) +
        QString("scroll_prsv:%1 ").arg(scroll_prsv) +
        QString("scroll_yoff:%1 ").arg(scroll_yoff) +
        QString("scroll_ymax:%1 ").arg(scroll_ymax) +
        QString("streamIdx:%1 ").arg(streamIdx));
#endif
    std::vector<CC608Text*>::iterator i;
    int visible = 0;

    m_state[streamIdx].m_output.m_lock.lock();
    if (!m_state[streamIdx].m_output.m_buffers.empty() && (scroll || replace))
    {
        // get last row
        int ylast = 0;
        i = m_state[streamIdx].m_output.m_buffers.end() - 1;
        CC608Text *cc = *i;
        if (cc)
            ylast = cc->m_y;

        // calculate row positions to delete, keep
        int ydel = scroll_yoff + scroll;
        int ykeep = scroll_ymax;
        int ymove = 0;
        if (scroll_prsv && ylast)
        {
            ymove = ylast - scroll_ymax;
            ydel += ymove;
            ykeep += ymove;
        }

        i = m_state[streamIdx].m_output.m_buffers.begin();
        while (i < m_state[streamIdx].m_output.m_buffers.end())
        {
            cc = (*i);
            if (!cc)
            {
                i = m_state[streamIdx].m_output.m_buffers.erase(i);
                continue;
            }

            if (cc->m_y > (ylast - replace))
            {
                // delete last lines
                delete cc;
                i = m_state[streamIdx].m_output.m_buffers.erase(i);
            }
            else if (scroll)
            {
                if (cc->m_y > ydel && cc->m_y <= ykeep)
                {
                    // scroll up
                    cc->m_y -= (scroll + ymove);
                    ++i;
                }
                else
                {
                    // delete lines outside scroll window
                    i = m_state[streamIdx].m_output.m_buffers.erase(i);
                    delete cc;
                }
            }
            else
            {
                ++i;
            }
        }
    }

    visible += m_state[streamIdx].m_output.m_buffers.size();

    if (ccbuf)
    {
        // add new text
        for (i = ccbuf->begin(); i != ccbuf->end(); ++i)
        {
            if (*i)
            {
                visible++;
                m_state[streamIdx].m_output.m_buffers.push_back(*i);
            }
        }
    }
    m_state[streamIdx].m_changed = (visible != 0);
    m_state[streamIdx].m_output.m_lock.unlock();
}

void CC608Reader::ClearBuffers(bool input, bool output, int outputStreamIdx)
{
    if (input)
    {
        for (int i = 0; i < MAXTBUFFER; i++)
        {
            m_inputBuffers[i].timecode = 0ms;
            if (m_inputBuffers[i].buffer)
                memset(m_inputBuffers[i].buffer, 0, m_maxTextSize);
        }

        QMutexLocker locker(&m_inputBufLock);
        m_readPosition  = 0;
        m_writePosition = 0;
    }

    if (output && outputStreamIdx < 0)
    {
        for (auto & state : m_state)
            state.Clear();
    }

    if (output && outputStreamIdx >= 0)
    {
        outputStreamIdx = std::min(outputStreamIdx, MAXOUTBUFFERS - 1);
        m_state[outputStreamIdx].Clear();
    }
}

int CC608Reader::NumInputBuffers(bool need_to_lock)
{
    int ret = 0;

    if (need_to_lock)
        m_inputBufLock.lock();

    if (m_readPosition >= m_writePosition)
        ret = m_readPosition - m_writePosition;
    else
        ret = MAXTBUFFER - (m_writePosition - m_readPosition);

    if (need_to_lock)
        m_inputBufLock.unlock();

    return ret;
}

void CC608Reader::AddTextData(unsigned char *buffer, int len,
                              std::chrono::milliseconds timecode, char type)
{
    if (m_parent)
        m_parent->WrapTimecode(timecode, TC_CC);

    if (!m_enabled)
        return;

    if (NumInputBuffers() >= MAXTBUFFER - 1)
    {
        LOG(VB_VBI, LOG_ERR, "AddTextData(): Text buffer overflow");
        return;
    }

    len = std::min(len, m_maxTextSize);

    QMutexLocker locker(&m_inputBufLock);
    int prev_readpos = (m_readPosition - 1 + MAXTBUFFER) % MAXTBUFFER;
    /* Check whether the reader appears to be waiting on a caption
       whose timestamp is too large.  We can guess this is the case
       if we are adding a timestamp that is smaller than timestamp
       being waited on but larger than the timestamp before that.
       Note that even if the text buffer is full, the entry at index
       m_readPosition-1 should still be valid.
    */
    if (NumInputBuffers(false) > 0 &&
        m_inputBuffers[m_readPosition].timecode > timecode &&
        timecode > m_inputBuffers[prev_readpos].timecode)
    {
        /* If so, reset the timestamp that the reader is waiting on
           to a value reasonably close to the previously read
           timestamp.  This will probably cause one or more captions
           to appear rapidly, but at least the captions won't
           appear to be stuck.
        */
        LOG(VB_VBI, LOG_INFO,
            QString("Writing caption timecode %1 but waiting on %2")
            .arg(timecode.count()).arg(m_inputBuffers[m_readPosition].timecode.count()));
        m_inputBuffers[m_readPosition].timecode =
            m_inputBuffers[prev_readpos].timecode + 500ms;
    }

    m_inputBuffers[m_readPosition].timecode = timecode;
    m_inputBuffers[m_readPosition].type     = type;
    m_inputBuffers[m_readPosition].len      = len;
    memset(m_inputBuffers[m_readPosition].buffer, 0, m_maxTextSize);
    memcpy(m_inputBuffers[m_readPosition].buffer, buffer, len);

    m_readPosition = (m_readPosition+1) % MAXTBUFFER;
}
