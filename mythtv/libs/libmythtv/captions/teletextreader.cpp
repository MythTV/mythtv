#include "captions/teletextreader.h"

#include <algorithm>
#include <cstring>

#include "captions/vbilut.h"
#include "tv.h"
#include "libmythui/mythuiactions.h"
#include "tv_actions.h"
#include "libmythbase/mythlogging.h"

static inline int MAGAZINE(int page) { return page / 256; };

TeletextReader::TeletextReader()
{
    m_bitswap.fill(0);
    for (int i = 0; i < 256; i++)
    {
        for (int bit = 0; bit < 8; bit++)
            if (i & (1 << bit))
                m_bitswap[i] |= (1 << (7-bit));
    }
    Reset();
}

bool TeletextReader::KeyPress(const QString &Key, bool& Exit)
{
    int newPage        = m_curpage;
    int newSubPage     = m_cursubpage;
    bool numeric_input = false;

    if (Key == "MENU" || Key == ACTION_TOGGLETT || Key == "ESCAPE")
    {
        Exit = true;
        return true;
    }

    TeletextSubPage *curpage = FindSubPage(m_curpage, m_cursubpage);

    if (Key == ACTION_0 || Key == ACTION_1 || Key == ACTION_2 ||
        Key == ACTION_3 || Key == ACTION_4 || Key == ACTION_5 ||
        Key == ACTION_6 || Key == ACTION_7 || Key == ACTION_8 ||
        Key == ACTION_9)
    {
        numeric_input = true;
        m_curpageShowHeader = true;
        if (m_pageinput[0] == ' ')
            m_pageinput[0] = '0' + Key.toInt();
        else if (m_pageinput[1] == ' ')
            m_pageinput[1] = '0' + Key.toInt();
        else if (m_pageinput[2] == ' ')
        {
            m_pageinput[2] = '0' + Key.toInt();
            newPage = ((m_pageinput[0] - '0') * 256) +
                      ((m_pageinput[1] - '0') * 16) +
                      (m_pageinput[2] - '0');
            newSubPage = -1;
        }
        else
        {
            m_pageinput[0] = '0' + Key.toInt();
            m_pageinput[1] = ' ';
            m_pageinput[2] = ' ';
        }

        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (Key == ACTION_NEXTPAGE)
    {
        TeletextPage *ttpage = FindPage(m_curpage, 1);
        if (ttpage)
            newPage = ttpage->pagenum;
        newSubPage = -1;
        m_curpageShowHeader = true;
    }
    else if (Key == ACTION_PREVPAGE)
    {
        TeletextPage *ttpage = FindPage(m_curpage, -1);
        if (ttpage)
            newPage = ttpage->pagenum;
        newSubPage = -1;
        m_curpageShowHeader = true;
    }
    else if (Key == ACTION_NEXTSUBPAGE)
    {
        TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, 1);
        if (ttpage)
            newSubPage = ttpage->subpagenum;
        m_curpageShowHeader = true;
    }
    else if (Key == ACTION_PREVSUBPAGE)
    {
        TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, -1);
        if (ttpage)
            newSubPage = ttpage->subpagenum;
        m_curpageShowHeader = true;
    }
    else if (Key == ACTION_TOGGLEBACKGROUND)
    {
        m_transparent = !m_transparent;
        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (Key == ACTION_REVEAL)
    {
        m_revealHidden = !m_revealHidden;
        PageUpdated(m_curpage, m_cursubpage);
    }
    else if (Key == ACTION_MENURED)
    {
        if (!curpage)
            return true;

        TeletextPage *page = FindPage(curpage->floflink[0]);
        if (page != nullptr)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpageShowHeader = true;
        }
    }
    else if (Key == ACTION_MENUGREEN)
    {
        if (!curpage)
            return true;

        TeletextPage *page = FindPage(curpage->floflink[1]);
        if (page != nullptr)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpageShowHeader = true;
        }
    }
    else if (Key == ACTION_MENUYELLOW)
    {
        if (!curpage)
            return true;

        TeletextPage *page = FindPage(curpage->floflink[2]);
        if (page != nullptr)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpageShowHeader = true;
        }
    }
    else if (Key == ACTION_MENUBLUE)
    {
        if (!curpage)
            return true;

        TeletextPage *page = FindPage(curpage->floflink[3]);
        if (page != nullptr)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpageShowHeader = true;
        }
    }
    else if (Key == ACTION_MENUWHITE)
    {
        if (!curpage)
            return true;

        TeletextPage *page = FindPage(curpage->floflink[4]);
        if (page != nullptr)
        {
            newPage = page->pagenum;
            newSubPage = -1;
            m_curpageShowHeader = true;
        }
    }
    else
    {
        return false;
    }

    newPage = std::clamp(newPage, 0x100, 0x899);

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

    int count = 1;
    int selected = 0;
    const TeletextPage *page = FindPage(m_curpage);
    if (page)
    {
        m_magazines[mag - 1].lock->lock();
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
            {
                str += " ";
            }

            str += QString("%1").arg(subpage->subpagenum,2,16,QChar('0'));

            ++subpageIter;
            ++count;
        }
        m_magazines[mag - 1].lock->unlock();
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
        startPos = std::max(startPos, 0);
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
    for (auto & mag : m_magazines)
    {
        QMutexLocker lock(mag.lock);

        // clear all sub pages in page
        int_to_page_t::iterator iter;
        iter = mag.pages.begin();
        while (iter != mag.pages.end())
        {
            TeletextPage *page = &iter->second;
            page->subpages.clear();
            ++iter;
        }

        // clear pages
        mag.pages.clear();
        mag.current_page = 0;
        mag.current_subpage = 0;
        mag.loadingpage.active = false;
    }
    m_header.fill(' ');

    m_curpage    = 0x100;
    m_cursubpage = -1;
    m_curpageShowHeader = true;

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

    for(int m = 1; m <= 8; m++)
    {
        // ETS 300 706, chapter 7.2.1:
        // The transmission of a given page begins with, and includes, its page
        // header packet. It is terminated by and excludes the next page header
        // packet having the same magazine address in parallel transmission
        // mode, or any magazine address in serial transmission mode.
        // ETS 300 706, chapter 9.3.1.3:
        // When set to '1' the service is designated to be in Serial mode and
        // the transmission of a page is terminated by the next page header with
        // a different page number.
        // When set to '0' the service is designated to be in Parallel mode and
        // the transmission of a page is terminated by the next page header with
        // a different page number but the same magazine number.  The same
        // setting shall be used for all page headers in the service.

        bool isMagazineSerialMode = (flags & TP_MAGAZINE_SERIAL) != 0;
        if (!(isMagazineSerialMode) && m != magazine)
        {
            continue;   // in parallel mode only process magazine
        }

        int lastPage = m_magazines[m - 1].current_page;
        int lastSubPage = m_magazines[m - 1].current_subpage;

        LOG(VB_VBI, LOG_DEBUG,
            QString("AddPageHeader(p %1, sp %2, lang %3, mag %4, lp %5, lsp %6"
                    " sm %7)")
            .arg(page).arg(subpage).arg(lang).arg(m).arg(lastPage)
            .arg(lastSubPage).arg(isMagazineSerialMode));

        if ((page != lastPage || subpage != lastSubPage) &&
            m_magazines[m - 1].loadingpage.active)
        {
            TeletextSubPage *ttpage = FindSubPage(lastPage, lastSubPage);
            if (!ttpage)
            {
                ttpage = &(m_magazines[m - 1]
                           .pages[lastPage].subpages[lastSubPage]);
                m_magazines[m - 1].pages[lastPage].pagenum = lastPage;
                ttpage->subpagenum = lastSubPage;
            }

            memcpy(ttpage, &m_magazines[m - 1].loadingpage,
                   sizeof(TeletextSubPage));

            m_magazines[m - 1].loadingpage.active = false;

            PageUpdated(lastPage, lastSubPage);
        }
    }

    m_fetchpage = page;
    m_fetchsubpage = subpage;

    TeletextSubPage *ttpage = &m_magazines[magazine - 1].loadingpage;

    m_magazines[magazine - 1].current_page = page;
    m_magazines[magazine - 1].current_subpage = subpage;

    for (auto & line : ttpage->data)
        line.fill(' ');

    ttpage->active = true;
    ttpage->subpagenum = subpage;
    ttpage->floflink.fill(0);
    ttpage->lang = lang;
    ttpage->flags = flags;
    ttpage->flof = 0;

    ttpage->subtitle = (vbimode == VBI_DVB_SUBTITLE);

    std::fill_n(ttpage->data[0].data(), 8, ' ');

    if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
    {
        for (uint j = 8; j < 40; j++)
            ttpage->data[0][j] = m_bitswap[buf[j]];
    }
    else
    {
        std::copy(buf, buf + 40, ttpage->data[0].data());
    }

    if ( !(ttpage->flags & TP_INTERRUPTED_SEQ))
    {
        std::copy(ttpage->data[0].cbegin(), ttpage->data[0].cend(), m_header.data());
        HeaderUpdated(page, subpage, ttpage->data[0],ttpage->lang);
    }
}

void TeletextReader::AddTeletextData(int magazine, int row,
                                     const uint8_t* buf, int vbimode)
{
    //LOG(VB_GENERAL, LOG_ERR, QString("AddTeletextData(%1, %2)")
    //    .arg(magazine).arg(row));

    int b1 = 0;
    int b2 = 0;
    int b3 = 0;
    int err = 0;

    if (magazine < 1 || magazine > 8)
        return;

    int currentpage = m_magazines[magazine - 1].current_page;
    if (!currentpage)
        return;

    TeletextSubPage *ttpage = &m_magazines[magazine - 1].loadingpage;

    switch (row)
    {
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
            if (b1 != 0 || !(b2 & 8))
                return;

            for (ptrdiff_t i = 0; i < 6; ++i)
            {
                err = 0;
                switch (vbimode)
                {
                    case VBI_IVTV:
                        b1 = hamm16(buf+1+(6*i), &err);
                        b2 = hamm16(buf+3+(6*i), &err);
                        b3 = hamm16(buf+5+(6*i), &err);
                        if (err & 0xF000)
                            return;
                        break;
                    case VBI_DVB:
                    case VBI_DVB_SUBTITLE:
                        b1 = (hamm84(buf+2+(6*i), &err) * 16) +
                        hamm84(buf+1+(6*i), &err);
                        b2 = (hamm84(buf+4+(6*i), &err) * 16) +
                        hamm84(buf+3+(6*i), &err);
                        b3 = (hamm84(buf+6+(6*i), &err) * 16) +
                        hamm84(buf+5+(6*i), &err);
                        if (err == 1)
                            return;
                        break;
                    default:
                        return;
                }

                int x = (b2 >> 7) | ((b3 >> 5) & 0x06);
                int nTmp = (magazine ^ x);
                ttpage->floflink[i] = (( nTmp ? nTmp : 8) * 256) + b1;
                ttpage->flof = 1;
            }
            break;

        case 31: // private streams
            break;

        default: /// other packet codes...

            if (( row >= 1 ) && ( row <= 24 ))  // Page Data
            {
                if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
                {
                    for (uint j = 0; j < 40; j++)
                        ttpage->data[row][j] = m_bitswap[buf[j]];
                }
                else
                {
                    std::copy(buf, buf + 40, ttpage->data[row].data());
                }
            }

            break;
    }
}

void TeletextReader::PageUpdated(int page, int subpage)
{
    if (page != m_curpage)
        return;
    if (subpage != m_cursubpage && m_cursubpage != -1)
        return;
    m_pageChanged = true;
}

void TeletextReader::HeaderUpdated(
    [[maybe_unused]] int page,
    [[maybe_unused]] int subpage,
    [[maybe_unused]] tt_line_array& page_ptr,
    [[maybe_unused]] int lang)
{
    if (!m_curpageShowHeader)
        return;

    m_headerChanged = true;
}

const TeletextPage *TeletextReader::FindPageInternal(
    int page, int direction) const
{
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return nullptr;

    QMutexLocker lock(m_magazines[mag - 1].lock);

    int_to_page_t::const_iterator pageIter;
    pageIter = m_magazines[mag - 1].pages.find(page);
    if (pageIter == m_magazines[mag - 1].pages.end())
        return nullptr;

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
        {
            res = &pageIter->second;
        }
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
        {
            res = &pageIter->second;
        }
    }

    return res;
}

const TeletextSubPage *TeletextReader::FindSubPageInternal(
                                int page, int subpage, int direction) const
{
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return nullptr;

    QMutexLocker lock(m_magazines[mag - 1].lock);

    int_to_page_t::const_iterator pageIter;
    pageIter = m_magazines[mag - 1].pages.find(page);
    if (pageIter == m_magazines[mag - 1].pages.end())
        return nullptr;

    const TeletextPage *ttpage = &(pageIter->second);
    auto subpageIter = ttpage->subpages.cbegin();

    // try to find the subpage given, or first if subpage == -1
    if (subpage != -1)
        subpageIter = ttpage->subpages.find(subpage);

    if (subpageIter == ttpage->subpages.cend())
        return nullptr;

    if (subpage == -1)
        return &(subpageIter->second);

    const TeletextSubPage *res = &(subpageIter->second);
    if (direction == -1)
    {
        --subpageIter;
        if (subpageIter == ttpage->subpages.cend())
        {
            auto iter = ttpage->subpages.crbegin();
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
        if (subpageIter == ttpage->subpages.cend())
            subpageIter = ttpage->subpages.cbegin();

        res = &(subpageIter->second);
    }

    return res;
}
