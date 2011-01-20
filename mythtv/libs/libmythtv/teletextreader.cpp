#include "teletextreader.h"

#include <string.h>
#include "vbilut.h"
#include "tv.h"

#define MAGAZINE(page) (page / 256)

TeletextReader::TeletextReader()
  : m_curpage(0x100),           m_cursubpage(-1),
    m_curpage_showheader(true), m_curpage_issubtitle(false),
    m_transparent(false),       m_revealHidden(false),
    m_header_changed(false),    m_page_changed(false),
    m_fetchpage(0),             m_fetchsubpage(0)
{
    memset(m_pageinput, 0, sizeof(m_pageinput));
    memset(m_header,    0, sizeof(m_header));
    for (int i = 0; i < 256; i++)
    {
        m_bitswap[i] = 0;
        for (int bit = 0; bit < 8; bit++)
            if (i & (1 << bit))
                m_bitswap[i] |= (1 << (7-bit));
    }
    Reset();
}

TeletextReader::~TeletextReader()
{
}

bool TeletextReader::KeyPress(const QString &key)
{
    int newPage        = m_curpage;
    int newSubPage     = m_cursubpage;
    bool numeric_input = false;

    TeletextSubPage *curpage = FindSubPage(m_curpage, m_cursubpage);
    TeletextPage *page;

    if (key == ACTION_0 || key == ACTION_1 || key == ACTION_2 ||
        key == ACTION_3 || key == ACTION_4 || key == ACTION_5 ||
        key == ACTION_6 || key == ACTION_7 || key == ACTION_8 ||
        key == ACTION_9)
    {
        numeric_input = true;
        m_curpage_showheader = true;
        if (m_pageinput[0] == ' ')
            m_pageinput[0] = '0' + key.toInt();
        else if (m_pageinput[1] == ' ')
            m_pageinput[1] = '0' + key.toInt();
        else if (m_pageinput[2] == ' ')
        {
            m_pageinput[2] = '0' + key.toInt();
            newPage = ((m_pageinput[0] - '0') * 256) +
                      ((m_pageinput[1] - '0') * 16) +
                      (m_pageinput[2] - '0');
            newSubPage = -1;
        }
        else
        {
            m_pageinput[0] = '0' + key.toInt();
            m_pageinput[1] = ' ';
            m_pageinput[2] = ' ';
        }

        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (key == ACTION_NEXTPAGE)
    {
        TeletextPage *ttpage = FindPage(m_curpage, 1);
        if (ttpage)
            newPage = ttpage->pagenum;
        newSubPage = -1;
        m_curpage_showheader = true;
    }
    else if (key == ACTION_PREVPAGE)
    {
        TeletextPage *ttpage = FindPage(m_curpage, -1);
        if (ttpage)
            newPage = ttpage->pagenum;
        newSubPage = -1;
        m_curpage_showheader = true;
    }
    else if (key == ACTION_NEXTSUBPAGE)
    {
        TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, 1);
        if (ttpage)
            newSubPage = ttpage->subpagenum;
        m_curpage_showheader = true;
    }
    else if (key == ACTION_PREVSUBPAGE)
    {
        TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, -1);
        if (ttpage)
            newSubPage = ttpage->subpagenum;
        m_curpage_showheader = true;
    }
    else if (key == ACTION_TOGGLEBACKGROUND)
    {
        m_transparent = !m_transparent;
        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (key == ACTION_REVEAL)
    {
        m_revealHidden = !m_revealHidden;
        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (key == ACTION_MENURED)
    {
        if (!curpage)
            return true;

        if ((page = FindPage(curpage->floflink[0])) != NULL)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
        }
    }
    else if (key == ACTION_MENUGREEN)
    {
        if (!curpage)
            return true;

        if ((page = FindPage(curpage->floflink[1])) != NULL)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
        }
    }
    else if (key == ACTION_MENUYELLOW)
    {
        if (!curpage)
            return true;

        if ((page = FindPage(curpage->floflink[2])) != NULL)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
        }
    }
    else if (key == ACTION_MENUBLUE)
    {
        if (!curpage)
            return true;

        if ((page = FindPage(curpage->floflink[3])) != NULL)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
        }
    }
    else if (key == ACTION_MENUWHITE)
    {
        if (!curpage)
            return true;

        if ((page = FindPage(curpage->floflink[4])) != NULL)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
        }
    }
    else
        return false;

    if (newPage < 0x100)
        newPage = 0x100;
    if (newPage > 0x899)
        newPage = 0x899;

    if (!numeric_input)
    {
        m_pageinput[0] = (newPage / 256) + '0';
        m_pageinput[1] = ((newPage % 256) / 16) + '0';
        m_pageinput[2] = (newPage % 16) + '0';
    }

    if (newPage != m_curpage || newSubPage != m_cursubpage)
    {
        m_curpage = newPage;
        m_cursubpage = newSubPage;
        m_revealHidden = false;
        PageUpdated(m_curpage, m_cursubpage);
    }

    return true;
}

QString TeletextReader::GetPage(void)
{
    QString str = "";
    int mag = MAGAZINE(m_curpage);
    if (mag > 8 || mag < 1)
        return str;

    int count = 1, selected = 0;
    const TeletextPage *page = FindPage(m_curpage);
    if (page)
    {
        m_magazines[mag - 1].lock.lock();
        int_to_subpage_t::const_iterator subpageIter;
        subpageIter = page->subpages.begin();
        while (subpageIter != page->subpages.end())
        {
            const TeletextSubPage *subpage = &subpageIter->second;

            if (subpage->subpagenum == m_cursubpage)
            {
                selected = count;
                str += "*";
            }
            else
                str += " ";

            str += QString().sprintf("%02X", subpage->subpagenum);

            ++subpageIter;
            ++count;
        }
        m_magazines[mag - 1].lock.unlock();
    }

    if (str.isEmpty())
        return str;

    // if there are less than 9 subpages fill the empty slots with spaces
    if (count < 10)
    {
        QString spaces;
        spaces.fill(' ', 27 - str.length());
        str = "  <" + str + spaces + " > ";
    }
    else
    {
        // try to centralize the selected sub page in the list
        int startPos = selected - 5;
        if (startPos < 0)
            startPos = 0;
        if (startPos + 9 >= count)
            startPos = count - 10;

        str = "  <" + str.mid(startPos * 3, 27) + " > ";
    }
    return str;
}

void TeletextReader::SetPage(int page, int subpage)
{
    if (page < 0x100 || page > 0x899)
        return;

    m_pageinput[0] = (page / 256) + '0';
    m_pageinput[1] = ((page % 256) / 16) + '0';
    m_pageinput[2] = (page % 16) + '0';

    m_curpage = page;
    m_cursubpage = subpage;
    PageUpdated(m_curpage, m_cursubpage);
}

void TeletextReader::Reset(void)
{
    for (uint mag = 0; mag < 8; mag++)
    {
        QMutexLocker lock(&m_magazines[mag].lock);

        // clear all sub pages in page
        int_to_page_t::iterator iter;
        iter = m_magazines[mag].pages.begin();
        while (iter != m_magazines[mag].pages.end())
        {
            TeletextPage *page = &iter->second;
            page->subpages.clear();
            ++iter;
        }

        // clear pages
        m_magazines[mag].pages.clear();
        m_magazines[mag].current_page = 0;
        m_magazines[mag].current_subpage = 0;
        m_magazines[mag].loadingpage.active = false;
    }
    memset(m_header, ' ', 40);

    m_curpage    = 0x100;
    m_cursubpage = -1;
    m_curpage_showheader = true;

    m_pageinput[0] = '1';
    m_pageinput[1] = '0';
    m_pageinput[2] = '0';
}

void TeletextReader::AddPageHeader(int page, int subpage, const uint8_t *buf,
                                   int vbimode, int lang, int flags)
{
    int magazine = MAGAZINE(page);
    if (magazine < 1 || magazine > 8)
        return;
    int lastPage = m_magazines[magazine - 1].current_page;
    int lastSubPage = m_magazines[magazine - 1].current_subpage;

    // update the last fetched page if the magazine is the same
    // and the page no. is different

    if ((page != lastPage || subpage != lastSubPage) &&
        m_magazines[magazine - 1].loadingpage.active)
    {
        TeletextSubPage *ttpage = FindSubPage(lastPage, lastSubPage);
        if (!ttpage)
        {
            ttpage = &(m_magazines[magazine - 1]
                       .pages[lastPage].subpages[lastSubPage]);
            m_magazines[magazine - 1].pages[lastPage].pagenum = lastPage;
            ttpage->subpagenum = lastSubPage;
        }

        memcpy(ttpage, &m_magazines[magazine - 1].loadingpage,
               sizeof(TeletextSubPage));

        m_magazines[magazine - 1].loadingpage.active = false;

        PageUpdated(lastPage, lastSubPage);
    }

    m_fetchpage = page;
    m_fetchsubpage = subpage;

    TeletextSubPage *ttpage = &m_magazines[magazine - 1].loadingpage;

    m_magazines[magazine - 1].current_page = page;
    m_magazines[magazine - 1].current_subpage = subpage;

    memset(ttpage->data, ' ', sizeof(ttpage->data));

    ttpage->active = true;
    ttpage->subpagenum = subpage;

    for (uint i = 0; i < 6; i++)
        ttpage->floflink[i] = 0;

    ttpage->lang = lang;
    ttpage->flags = flags;
    ttpage->flof = 0;

    ttpage->subtitle = (vbimode == VBI_DVB_SUBTITLE);

    memset(ttpage->data[0], ' ', 8 * sizeof(uint8_t));

    if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
    {
        for (uint j = 8; j < 40; j++)
            ttpage->data[0][j] = m_bitswap[buf[j]];
    }
    else
    {
        memcpy(ttpage->data[0]+0, buf, 40);
    }

    if ( !(ttpage->flags & TP_INTERRUPTED_SEQ))
    {
        memcpy(m_header, ttpage->data[0], 40);
        HeaderUpdated(ttpage->data[0],ttpage->lang);
    }
}

void TeletextReader::AddTeletextData(int magazine, int row,
                                     const uint8_t* buf, int vbimode)
{
    int b1, b2, b3, err = 0;

    if (magazine < 1 || magazine > 8)
        return;

    int currentpage = m_magazines[magazine - 1].current_page;
    if (!currentpage)
        return;

    TeletextSubPage *ttpage = &m_magazines[magazine - 1].loadingpage;

    switch (row)
    {
        case 1 ... 24: // Page Data
            if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
            {
                for (uint j = 0; j < 40; j++)
                    ttpage->data[row][j] = m_bitswap[buf[j]];
            }
            else
            {
                memcpy(ttpage->data[row], buf, 40);
            }
            break;
        case 26:
            /* XXX TODO: Level 1.5, 2.5, 3.5
            *      Character location & override
            * Level 2.5, 3.5
            *      Modifying display attributes
            * All levels
            *      VCR Programming
            * See 12.3
            */
            break;
        case 27: // FLOF data (FastText)
            switch (vbimode)
            {
                case VBI_IVTV:
                    b1 = hamm8(buf, &err);
                    b2 = hamm8(buf + 37, &err);
                    if (err & 0xF000)
                        return;
                     break;
                case VBI_DVB:
                case VBI_DVB_SUBTITLE:
                    b1 = hamm84(buf, &err);
                    b2 = hamm84(buf + 37, &err);
                    if (err == 1)
                        return;
                    break;
                default:
                    return;
            }
            if (b1 != 0 || not(b2 & 8))
                return;

            for (int i = 0; i < 6; ++i)
            {
                err = 0;
                switch (vbimode)
                {
                    case VBI_IVTV:
                        b1 = hamm16(buf+1+6*i, &err);
                        b2 = hamm16(buf+3+6*i, &err);
                        b3 = hamm16(buf+5+6*i, &err);
                        if (err & 0xF000)
                            return;
                        break;
                    case VBI_DVB:
                    case VBI_DVB_SUBTITLE:
                        b1 = hamm84(buf+2+6*i, &err) * 16 +
                        hamm84(buf+1+6*i, &err);
                        b2 = hamm84(buf+4+6*i, &err) * 16 +
                        hamm84(buf+3+6*i, &err);
                        b3 = hamm84(buf+6+6*i, &err) * 16 +
                        hamm84(buf+5+6*i, &err);
                        if (err == 1)
                            return;
                        break;
                    default:
                        return;
                }

                int x = (b2 >> 7) | ((b3 >> 5) & 0x06);
                ttpage->floflink[i] = ((magazine ^ x) ?: 8) * 256 + b1;
                ttpage->flof = 1;
            }
            break;

        case 31: // private streams
            break;

        default: /// other packet codes...
            break;
    }
}

void TeletextReader::PageUpdated(int page, int subpage)
{
    if (page != m_curpage)
        return;
    if (subpage != m_cursubpage && m_cursubpage != -1)
        return;
    m_page_changed = true;
}

void TeletextReader::HeaderUpdated(uint8_t * page, int lang)
{
    (void)lang;

    if (page == NULL)
        return;

    if (m_curpage_showheader == false)
        return;

    m_header_changed = true;
}

const TeletextPage *TeletextReader::FindPageInternal(
    int page, int direction) const
{
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return NULL;

    QMutexLocker lock(&m_magazines[mag - 1].lock);

    int_to_page_t::const_iterator pageIter;
    pageIter = m_magazines[mag - 1].pages.find(page);
    if (pageIter == m_magazines[mag - 1].pages.end())
        return NULL;

    const TeletextPage *res = &pageIter->second;
    if (direction == -1)
    {
        --pageIter;
        if (pageIter == m_magazines[mag - 1].pages.end())
        {
            int_to_page_t::const_reverse_iterator iter;
            iter = m_magazines[mag - 1].pages.rbegin();
            res = &iter->second;
        }
        else
            res = &pageIter->second;
    }

    if (direction == 1)
    {
        ++pageIter;
        if (pageIter == m_magazines[mag - 1].pages.end())
        {
            pageIter = m_magazines[mag - 1].pages.begin();
            res = &pageIter->second;
        }
        else
            res = &pageIter->second;
    }

    return res;
}

const TeletextSubPage *TeletextReader::FindSubPageInternal(
                                int page, int subpage, int direction) const
{
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return NULL;

    QMutexLocker lock(&m_magazines[mag - 1].lock);

    int_to_page_t::const_iterator pageIter;
    pageIter = m_magazines[mag - 1].pages.find(page);
    if (pageIter == m_magazines[mag - 1].pages.end())
        return NULL;

    const TeletextPage *ttpage = &(pageIter->second);
    int_to_subpage_t::const_iterator subpageIter =
        ttpage->subpages.begin();

    // try to find the subpage given, or first if subpage == -1
    if (subpage != -1)
        subpageIter = ttpage->subpages.find(subpage);

    if (subpageIter == ttpage->subpages.end())
        return NULL;

    if (subpage == -1)
        return &(subpageIter->second);

    const TeletextSubPage *res = &(subpageIter->second);
    if (direction == -1)
    {
        --subpageIter;
        if (subpageIter == ttpage->subpages.end())
        {
            int_to_subpage_t::const_reverse_iterator iter =
                ttpage->subpages.rbegin();
            res = &(iter->second);
        }
        else
        {
            res = &(subpageIter->second);
        }
    }

    if (direction == 1)
    {
        ++subpageIter;
        if (subpageIter == ttpage->subpages.end())
            subpageIter = ttpage->subpages.begin();

        res = &(subpageIter->second);
    }

    return res;
}
