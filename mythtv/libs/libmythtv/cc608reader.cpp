extern "C" {
#include "vbitext/vbi.h"
}

#include "mythplayer.h"
#include "cc608reader.h"

CC608Reader::CC608Reader(MythPlayer *parent)
  : m_parent(parent),   m_enabled(false), m_readPosition(0),
    m_writePosition(0), m_maxTextSize(0), m_ccMode(CC_CC1),
    m_ccPageNum(0x888), m_outputText(""), m_outputCol(0),
    m_outputRow(0),     m_changed(true)
{
    bzero(&m_inputBuffers, sizeof(m_inputBuffers));
    m_maxTextSize = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
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
            m_inputBuffers[i].buffer = NULL;
        }
    }
}

void CC608Reader::FlushTxtBuffers(void)
{
    QMutexLocker locker(&m_inputBufLock);
    m_readPosition = m_writePosition;
}

CC608Buffer* CC608Reader::GetOutputText(bool &changed)
{
    if (!m_enabled || !m_parent)
        return NULL;

    VideoFrame *last = NULL;
    if (m_parent->getVideoOutput())
        last = m_parent->getVideoOutput()->GetLastShownFrame();

    if (NumInputBuffers() && m_inputBuffers[m_writePosition].timecode &&
       (last && m_inputBuffers[m_writePosition].timecode <= last->timecode))
    {
        if (m_inputBuffers[m_writePosition].type == 'T')
        {
            // display full page of teletext
            //
            // all formatting is always defined in the page itself,
            // if scrolling is needed for live closed captions this
            // is handled by the broadcaster:
            // the pages are then very often transmitted (sometimes as often as
            // every 2 frames) with small differences between them
            unsigned char *inpos = m_inputBuffers[m_writePosition].buffer;
            int pagenr;
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);

            if (pagenr == (m_ccPageNum<<16))
            {
                // show teletext subtitles
                ClearBuffers(false, true);
                (*inpos)++;
                while (*inpos)
                {
                    struct teletextsubtitle st;
                    memcpy(&st, inpos, sizeof(st));
                    inpos += sizeof(st);

                    CC608Text *cc = new CC608Text(QString((const char*) inpos),
                                        st.row, st.col, st.fg, true);
                    m_outputBuffers.lock.lock();
                    m_outputBuffers.buffers.push_back(cc);
                    m_outputBuffers.lock.unlock();
                    inpos += st.len;
                }
                changed = true;
            }
        }
        else if (m_inputBuffers[m_writePosition].type == 'C')
        {
            Update(m_inputBuffers[m_writePosition].buffer);
            changed = true;
        }

        QMutexLocker locker(&m_inputBufLock);
        if (m_writePosition != m_readPosition)
            m_writePosition = (m_writePosition + 1) % MAXTBUFFER;
    }

    m_changed = false;
    return &m_outputBuffers;
}

void CC608Reader::SetMode(int mode)
{
    int oldmode = m_ccMode;
    m_ccMode = (mode <= 2) ? ((mode == 1) ? CC_CC1 : CC_CC2) :
                           ((mode == 3) ? CC_CC3 : CC_CC4);
    if (oldmode != m_ccMode)
        ClearBuffers(true, true);
}

void CC608Reader::Update(unsigned char *inpos)
{
    struct ccsubtitle subtitle;

    memcpy(&subtitle, inpos, sizeof(subtitle));
    inpos += sizeof(ccsubtitle);

    // skip undisplayed streams
    if ((subtitle.resumetext & CC_MODE_MASK) != m_ccMode)
        return;

    if (subtitle.row == 0)
        subtitle.row = 1;

    if (subtitle.clr)
    {
        //printf ("erase displayed memory\n");
        ClearBuffers(false, true);
        if (!subtitle.len)
            return;
    }

//    if (subtitle.len || !subtitle.clr)
    {
        unsigned char *end = inpos + subtitle.len;
        int row = 0;
        int linecont = (subtitle.resumetext & CC_LINE_CONT);

        vector<CC608Text*> *ccbuf = new vector<CC608Text*>;
        vector<CC608Text*>::iterator ccp;
        CC608Text *tmpcc = NULL;
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
                    m_outputText.remove(m_outputText.length() - bscnt, bscnt);
            }
            else
            {
                // new line:  count spaces to calculate column position
                row++;
                m_outputCol = 0;
                m_outputText = "";
                while ((inpos < end) && *inpos != 0 && (char)*inpos == ' ')
                {
                    inpos++;
                    m_outputCol++;
                }
            }

            m_outputRow = subtitle.row;
            unsigned char *cur = inpos;

            //null terminate at EOL
            while (cur < end && *cur != '\n' && *cur != 0)
                cur++;
            *cur = 0;

            if (*inpos != 0 || linecont)
            {
                if (linecont)
                    m_outputText += QString::fromUtf8((const char *)inpos, -1);
                else
                    m_outputText = QString::fromUtf8((const char *)inpos, -1);
                tmpcc = new CC608Text(m_outputText, m_outputCol, m_outputRow,
                                      0, false);
                ccbuf->push_back(tmpcc);
#if 0
                if (ccbuf->size() > 4)
                {
                    QByteArray ccl = m_outputText.toAscii();
                    printf("CC overflow:  ");
                    printf("%d %d %s\n", m_outputCol, m_outputRow, ccl.constData());
                }
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
            if (m_outputRow > 15)
            {
                if (row)
                    scroll = m_outputRow - 15;
                if (tmpcc)
                    tmpcc->y = 15;
            }
        }
        else if (subtitle.rowcount == 0 || row > 1)
        {
            // multi-line text
            // - fix display of old (badly-encoded) files
            if (m_outputRow > 15)
            {
                ccp = ccbuf->begin();
                for (; ccp != ccbuf->end(); ccp++)
                {
                    tmpcc = *ccp;
                    tmpcc->y -= (m_outputRow - 15);
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
            if (subtitle.rowcount > 4)
                subtitle.rowcount = 4;
            if (m_outputRow < subtitle.rowcount)
            {
                m_outputRow = subtitle.rowcount;
                if (tmpcc)
                    tmpcc->y = m_outputRow;
            }
            if (row)
            {
                scroll = row;
                scroll_prsv = true;
                scroll_yoff = m_outputRow - subtitle.rowcount;
                scroll_ymax = m_outputRow;
            }
        }

        Update608Text(ccbuf, replace, scroll,
                      scroll_prsv, scroll_yoff, scroll_ymax);
        delete ccbuf;
    }
}

void CC608Reader::TranscodeWriteText(void (*func)
                                    (void *, unsigned char *, int, int, int),
                                     void * ptr)
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

void CC608Reader::Update608Text(vector<CC608Text*> *ccbuf,
                              int replace, int scroll, bool scroll_prsv,
                              int scroll_yoff, int scroll_ymax)
// ccbuf      :  new text
// replace    :  replace last lines
// scroll     :  scroll amount
// scroll_prsv:  preserve last lines and move into scroll window
// scroll_yoff:  yoff < scroll window <= ymax
// scroll_ymax:
{
    vector<CC608Text*>::iterator i;
    int visible = 0;

    m_outputBuffers.lock.lock();
    if (m_outputBuffers.buffers.size() && (scroll || replace))
    {
        CC608Text *cc;

        // get last row
        int ylast = 0;
        i = m_outputBuffers.buffers.end() - 1;
        cc = *i;
        if (cc)
            ylast = cc->y;

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

        i = m_outputBuffers.buffers.begin();
        while (i < m_outputBuffers.buffers.end())
        {
            cc = (*i);
            if (!cc)
            {
                i = m_outputBuffers.buffers.erase(i);
                continue;
            }

            if (cc->y > (ylast - replace))
            {
                // delete last lines
                delete cc;
                i = m_outputBuffers.buffers.erase(i);
            }
            else if (scroll)
            {
                if (cc->y > ydel && cc->y <= ykeep)
                {
                    // scroll up
                    cc->y -= (scroll + ymove);
                    i++;
                }
                else
                {
                    // delete lines outside scroll window
                    i = m_outputBuffers.buffers.erase(i);
                    delete cc;
                }
            }
            else
                i++;
        }
    }

    visible += m_outputBuffers.buffers.size();

    if (ccbuf)
    {
        // add new text
        i = ccbuf->begin();
        while (i < ccbuf->end())
        {
            if (*i)
            {
                visible++;
                m_outputBuffers.buffers.push_back(*i);
            }
            i++;
        }
    }
    m_changed = visible;
    m_outputBuffers.lock.unlock();
}

void CC608Reader::ClearBuffers(bool input, bool output)
{

    if (input)
    {
        for (int i = 0; i < MAXTBUFFER; i++)
        {
            m_inputBuffers[i].timecode = 0;
            if (m_inputBuffers[i].buffer)
                memset(m_inputBuffers[i].buffer, 0, m_maxTextSize);
        }

        QMutexLocker locker(&m_inputBufLock);
        m_readPosition  = 0;
        m_writePosition = 0;
    }

    if (output)
    {
        m_outputText = "";
        m_outputCol  = 0;
        m_outputRow  = 0;
        m_outputBuffers.Clear();
        m_changed = true;
    }
}

int CC608Reader::NumInputBuffers(bool need_to_lock)
{
    int ret;

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
                              int64_t timecode, char type)
{
    if (m_parent)
        m_parent->WrapTimecode(timecode, TC_CC);

    if (!m_enabled)
        return;

    if (!(MAXTBUFFER - NumInputBuffers() - 1))
    {
        VERBOSE(VB_IMPORTANT, "NVP::AddTextData(): Text buffer overflow");
        return;
    }

    if (len > m_maxTextSize)
        len = m_maxTextSize;

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
        VERBOSE(VB_VBI,
                QString("Writing caption timecode %1 but waiting on %2")
                .arg(timecode).arg(m_inputBuffers[m_readPosition].timecode));
        m_inputBuffers[m_readPosition].timecode =
            m_inputBuffers[prev_readpos].timecode + 500;
    }

    m_inputBuffers[m_readPosition].timecode = timecode;
    m_inputBuffers[m_readPosition].type     = type;
    m_inputBuffers[m_readPosition].len      = len;
    memset(m_inputBuffers[m_readPosition].buffer, 0, m_maxTextSize);
    memcpy(m_inputBuffers[m_readPosition].buffer, buffer, len);

    m_readPosition = (m_readPosition+1) % MAXTBUFFER;
}
