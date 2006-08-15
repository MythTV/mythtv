/* =============================================================
 * File  : osdtypeteletext.cpp
 * Author: Frank Muenchow <beebof@gmx.de>
 *         Martin Barnasconi
 * Date  : 2005-10-25
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * =============================================================
 */

#include <cmath>

#include <qmutex.h>

#include "osd.h"
#include "osdsurface.h"
#include "osdtypes.h"
#include "osdtypeteletext.h"
#include "ttfont.h"
#include "vbilut.h"

const QColor OSDTypeTeletext::kColorBlack      = QColor(  0,  0,  0);
const QColor OSDTypeTeletext::kColorRed        = QColor(255,  0,  0);
const QColor OSDTypeTeletext::kColorGreen      = QColor(  0,255,  0);
const QColor OSDTypeTeletext::kColorYellow     = QColor(255,255,  0);
const QColor OSDTypeTeletext::kColorBlue       = QColor(  0,  0,255);
const QColor OSDTypeTeletext::kColorMagenta    = QColor(255,  0,255);
const QColor OSDTypeTeletext::kColorCyan       = QColor(  0,255,255);
const QColor OSDTypeTeletext::kColorWhite      = QColor(255,255,255);
const int    OSDTypeTeletext::kTeletextColumns = 40;
const int    OSDTypeTeletext::kTeletextRows    = 26;

/*****************************************************************************/
OSDTypeTeletext::OSDTypeTeletext(const QString &name, TTFFont *font,
                                 QRect displayrect, float wmult, float hmult,
                                 OSD *osd)
    : OSDType(name),
      m_lock(true),
      m_displayrect(displayrect),       m_unbiasedrect(0,0,0,0),
      m_box(NULL),

      m_tt_colspace(m_displayrect.width()  / kTeletextColumns),
      m_tt_rowspace(m_displayrect.height() / kTeletextRows),

      // last fetched page
      m_fetchpage(0),                   m_fetchsubpage(0),

      // current font
      m_font(font),

      // current character background
      m_bgcolor_y(0),                   m_bgcolor_u(0),
      m_bgcolor_v(0),                   m_bgcolor_a(0),

      // currently displayed page
      m_curpage(0x100),                 m_cursubpage(-1),
      m_curpage_showheader(true),       m_curpage_issubtitle(false),

      m_transparent(false),             m_revealHidden(false),
      m_displaying(false),              m_osd(osd),
      m_header_changed(false),          m_page_changed(false)
{
    m_unbiasedrect  = bias(m_displayrect, wmult, hmult);

    // fill Bitswap
    for (int i = 0; i < 256; i++)
    {
        m_bitswap[i] = 0;
        for (int bit = 0; bit < 8; bit++)
            if (i & (1 << bit))
                m_bitswap[i] |= (1 << (7-bit));
    }

    Reset(); // initializes m_magazines
}

/** \fn OSDTypeTeletext::Reset(void)
 *  \brief Resets the teletext data cache
 */
void OSDTypeTeletext::Reset(void)
{
    QMutexLocker locker(&m_lock);

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

/** \fn OSDTypeTeletext::AddPageHeader(int,int,const unsigned char*,int,int,int)
 *  \brief Adds a new page header (page=0 if none).
 */
void OSDTypeTeletext::AddPageHeader(int page, int subpage,
                                    const unsigned char* buf,
                                    int vbimode, int lang, int flags)
{
    QMutexLocker locker(&m_lock);

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

/** \fn OSDTypeTeletext::AddTeletextData(int,int,const unsigned char*,int)
 *  \brief Adds Teletext Data from TeletextDecoder
 */
void OSDTypeTeletext::AddTeletextData(int magazine, int row, 
                                      const unsigned char* buf, int vbimode)
{
    QMutexLocker locker(&m_lock);

    int b1, b2, b3, err;

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
                    b2 = hamm8(buf + 37, & err);
                    if (err & 0xF00)
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

/** \fn OSDTypeTeletext::PageUpdated(int,int)
 *  \brief Updates the page, if given pagenumber is the same
 *         as the current shown page
 */
void OSDTypeTeletext::PageUpdated(int page, int subpage)
{
    if (!m_displaying)
        return;

    if (page != m_curpage)
        return;

    if (subpage != m_cursubpage && m_cursubpage != -1)
        return;

    m_page_changed = true;
    m_osd->UpdateTeletext();
}

/** \fn OSDTypeTeletext::HeaderUpdated(unsigned char*,int)
 *  \brief Updates the header (given in page with language lang)
 * 
 *  \param page Pointer to the header which should be displayed
 *  \param lang Language of the header
 *
 */
void OSDTypeTeletext::HeaderUpdated(unsigned char *page, int lang)
{
    (void)lang;

    if (!m_displaying)
        return;

    if (page == NULL)
        return;

    if (m_curpage_showheader == false)
        return;

    m_header_changed = true;

    // Cannot use the OSD to trigger a draw for this. If it does it
    // takes too much time drawing the page each time the data changes
    // (the OSD erases the surface before doing a redraw so the entire
    // teletext contents must be re-displayed).
    //m_osd->UpdateTeletext();
}

/** \fn OSDTypeTeletext::FindPageInternal(int, int)
 *  \brief Finds the given page
 *
 *  \param page Page number
 *  \param direction find page before or after the given page
 *  \return TeletextPage (NULL if not found) 
 */
const TeletextPage *OSDTypeTeletext::FindPageInternal(
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

/** \fn OSDTypeTeletext::FindSubPageInternal(int, int, int) const
 *  \brief Finds the given page
 *
 *  \param page Page number
 *  \param subpage Subpage number (if set to -1, find the first subpage)
 *  \param direction find page before or after the given page
 *         (only if Subpage is not -1)
 *  \return TeletextSubPage (NULL if not found) 
 */
const TeletextSubPage *OSDTypeTeletext::FindSubPageInternal(
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

/** \fn OSDTypeTeletext::KeyPress(uint)
 *  \brief What to do if a key is pressed
 *
 *  \param key pressed key
 *  \sa TTKey
 */
void OSDTypeTeletext::KeyPress(uint key)
{
    QMutexLocker locker(&m_lock);

    int newPage = m_curpage;
    int newSubPage = m_cursubpage;
    bool numeric_input = false;

    TeletextSubPage *curpage = FindSubPage(m_curpage, m_cursubpage);
    TeletextPage *page;

    switch (key)
    {
        case TTKey::k0 ... TTKey::k9:
            numeric_input = true;
            m_curpage_showheader = true;
            if (m_pageinput[0] == ' ')
                m_pageinput[0] = '0' + static_cast<int> (key);
            else if (m_pageinput[1] == ' ')
                m_pageinput[1] = '0' + static_cast<int> (key);
            else if (m_pageinput[2] == ' ')
            {
                m_pageinput[2] = '0' + static_cast<int> (key);
                newPage = ((m_pageinput[0] - '0') * 256) +
                    ((m_pageinput[1] - '0') * 16) +
                    (m_pageinput[2] - '0');
                newSubPage = -1;
            }
            else
            {
                m_pageinput[0] = '0' + static_cast<int> (key);
                m_pageinput[1] = ' ';
                m_pageinput[2] = ' ';
            }

            break;

        case TTKey::kNextPage:
        {
            TeletextPage *ttpage = FindPage(m_curpage, 1);
            if (ttpage)
                newPage = ttpage->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
            break;
        } 

        case TTKey::kPrevPage:
        {
            TeletextPage *ttpage = FindPage(m_curpage, -1);
            if (ttpage)
                newPage = ttpage->pagenum;
            newSubPage = -1;
            m_curpage_showheader = true;
            break;
        }

        case TTKey::kNextSubPage:
        {
            TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, 1);
            if (ttpage)
                newSubPage = ttpage->subpagenum;
            m_curpage_showheader = true;
            break;
        }

        case TTKey::kPrevSubPage:
        {
            TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage, -1);
            if (ttpage)
                newSubPage = ttpage->subpagenum;
            m_curpage_showheader = true;
            break;
        }

        case TTKey::kHold:
            break;

        case TTKey::kTransparent:
            m_transparent = !m_transparent;
            PageUpdated(m_curpage, m_cursubpage);
            break;

        case TTKey::kRevealHidden:
            m_revealHidden = !m_revealHidden;
            PageUpdated(m_curpage, m_cursubpage);
            break;

        case TTKey::kFlofRed:
        {
            if (!curpage)
                return;

            if ((page = FindPage(curpage->floflink[0])) != NULL)
            {
                newPage = page->pagenum;
                newSubPage = -1;
                m_curpage_showheader = true;
            }
            break;
        }

        case TTKey::kFlofGreen:
        {
            if (!curpage)
                return;

            if ((page = FindPage(curpage->floflink[1])) != NULL)
            {
                newPage = page->pagenum;
                newSubPage = -1;
                m_curpage_showheader = true;
            }
            break;
        }

        case TTKey::kFlofYellow:
        {
            if (!curpage)
                return;

            if ((page = FindPage(curpage->floflink[2])) != NULL)
            {
                newPage = page->pagenum;
                newSubPage = -1;
                m_curpage_showheader = true;
            }
            break;
        }

        case TTKey::kFlofBlue:
        {
            if (!curpage)
                return;

            if ((page = FindPage(curpage->floflink[3])) != NULL)
            {
                newPage = page->pagenum;
                newSubPage = -1;
                m_curpage_showheader = true;
            }
            break;
        }

        case TTKey::kFlofWhite:
        {
            if (!curpage)
                return;

            if ((page = FindPage(curpage->floflink[4])) != NULL)
            {
                newPage = page->pagenum;
                newSubPage = -1;
                m_curpage_showheader = true;
            }
            break;
        }
    }

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
}

/** \fn OSDTypeTeletext::SetPage(uint)
 *  \brief Set TeletextPage to display
 *  \param page PageNr. (in hex)
 *  \param subpage Subpage (-1 if first available)
 */
void OSDTypeTeletext::SetPage(int page, int subpage)
{
    QMutexLocker locker(&m_lock);

    if (page < 0x100 || page > 0x899)
        return;

    m_pageinput[0] = (page / 256) + '0';
    m_pageinput[1] = ((page % 256) / 16) + '0';
    m_pageinput[2] = (page % 16) + '0';

    m_curpage = page;
    m_cursubpage = subpage;
    PageUpdated(m_curpage, m_cursubpage);
}

QColor color_tt2qt(int ttcolor)
{
    QColor color;

    switch (ttcolor & ~TTColor::TRANSPARENT)
    {
        case TTColor::BLACK:   color = OSDTypeTeletext::kColorBlack;   break;
        case TTColor::RED:     color = OSDTypeTeletext::kColorRed;     break;
        case TTColor::GREEN:   color = OSDTypeTeletext::kColorGreen;   break;
        case TTColor::YELLOW:  color = OSDTypeTeletext::kColorYellow;  break;
        case TTColor::BLUE:    color = OSDTypeTeletext::kColorBlue;    break;
        case TTColor::MAGENTA: color = OSDTypeTeletext::kColorMagenta; break;
        case TTColor::CYAN:    color = OSDTypeTeletext::kColorCyan;    break;
        case TTColor::WHITE:   color = OSDTypeTeletext::kColorWhite;   break;
    }

    return color;
}

/** \fn OSDTypeTeletext::SetForegroundColor(int) const
 *  \brief Set the font color to the given color.
 *
 *   NOTE: TTColor::TRANSPARENT does not do anything!
 *  \sa TTColor
 */
void OSDTypeTeletext::SetForegroundColor(int ttcolor) const
{
    m_font->setColor(color_tt2qt(ttcolor));
    m_font->setShadow(0,0);
    m_font->setOutline(0);
}

/** \fn OSDTypeTeletext:SetBackgroundColor(int) const
 *  \brief Set the background color to the given color
 *
 *  \sa TTColor
 */
void OSDTypeTeletext::SetBackgroundColor(int ttcolor) const
{
    const QColor color = color_tt2qt(ttcolor);

    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();

    const float y = (0.299*r) + (0.587*g) + (0.114*b);
    const float u = (0.564*(b - y)); // = -0.169R-0.331G+0.500B
    const float v = (0.713*(r - y)); // = 0.500R-0.419G-0.081B

    m_bgcolor_y = (uint8_t)(y);
    m_bgcolor_u = (uint8_t)(127 + u);
    m_bgcolor_v = (uint8_t)(127 + v);
    m_bgcolor_a = (ttcolor & TTColor::TRANSPARENT) ? 0x00 : 0xff;
}

/** \fn OSDTypeTeletext::DrawBackground(OSDSurface*,int,int) const
 *  \brief draws background in the color set in SetBackgroundColor().
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 */
void OSDTypeTeletext::DrawBackground(OSDSurface *surface, int x, int y) const
{
    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    DrawRect(surface, QRect(x, y, m_tt_colspace, m_tt_rowspace));
}

/** \fn OSDTypeteletext::DrawRect(OSDSurface*,const QRect) const
 *  \brief Draws a Rectangle with the color set in SetBackgroundColor().
 */
void OSDTypeTeletext::DrawRect(OSDSurface *surface, const QRect rect) const
{
    QRect tmp = rect;
    surface->AddRect(tmp);

    const int luma_stride   = surface->width;
    const int chroma_stride = surface->width >> 1;
    const int y  = rect.top(),    x  = rect.left();
    const int dy = rect.height(), dx = rect.width();
    const int ye = y + dy;

    unsigned char *buf_y = surface->y + (luma_stride * y) + x;
    unsigned char *buf_u = surface->u + (chroma_stride * (y>>1)) + (x>>1);
    unsigned char *buf_v = surface->v + (chroma_stride * (y>>1)) + (x>>1);
    unsigned char *buf_a = surface->alpha + (luma_stride * y) + x;

    for (int j = y; j < ye; j++)
    {
        for (int i = 0; i < dx; i++)
        {
            buf_y[i] = m_bgcolor_y;
            buf_a[i] = m_bgcolor_a;
        }

        if (!(j & 1))
        {
            for (int k = 0; k < dx; k++)
            {
                buf_u[k>>1] = m_bgcolor_u;
                buf_v[k>>1] = m_bgcolor_v;
            }

            buf_u += chroma_stride;
            buf_v += chroma_stride;
        }

        buf_y += luma_stride;
        buf_a += luma_stride;
    }
}

/** \fn OSDTypeTeletext::DrawCharacter(OSDSurface*,int,int,QChar,int) const
 *  \brief Draws a character at posistion x, y 
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 *  \param ch Character
 *  \param doubleheight if different to 0, draw doubleheighted character
 */
void OSDTypeTeletext::DrawCharacter(OSDSurface *surface,
                                    int x, int y,
                                    QChar ch, int doubleheight) const
{
    if (!m_font)
        return;

    QString line = ch;

    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    m_font->DrawString(surface, x, y, line,
                       surface->width, surface->height,
                       255, doubleheight);
}

/** \fn OSDTypeTeletext::DrawMosaic(int, int, code, int)
 *  \brief Draws a mosaic as defined in ETSI EN 300 706
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 *  \param code Code
 *  \param doubleheight if different to 0, draw doubleheighted mosaic) const
 */
void OSDTypeTeletext::DrawMosaic(OSDSurface *surface, int x, int y,
                                 int code, int doubleheight) const
{
    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    int dx = (int)round(m_tt_colspace / 2) + 1;
    int dy = (int)round(m_tt_rowspace / 3) + 1;
    dy = (doubleheight) ? (2 * dy) : dy;

    if (code & 0x10)
        DrawRect(surface, QRect(x,      y + 2*dy, dx, dy));
    if (code & 0x40)
        DrawRect(surface, QRect(x + dx, y + 2*dy, dx, dy));
    if (code & 0x01)
        DrawRect(surface, QRect(x,      y,        dx, dy));
    if (code & 0x02)
        DrawRect(surface, QRect(x + dx, y,        dx, dy));
    if (code & 0x04)
        DrawRect(surface, QRect(x,      y + dy,   dx, dy));
    if (code & 0x08)
        DrawRect(surface, QRect(x + dx, y + dy,   dx, dy));
}

/** \fn cvt_char(char,int)
 *  \brief converts the given character to the given language
 */
static char cvt_char(char ch, int lang)
{
    int c = 0;
    for (int j = 0; j < 14; j++)
    {
        c = ch & 0x7F;
        if (c == lang_chars[0][j])
            ch = lang_chars[lang + 1][j];
    }
    return ch;
}

void OSDTypeTeletext::DrawLine(OSDSurface *surface, const unsigned char *page,
                               uint row, int lang) const
{
    bool mosaic;
    bool conceal;
    bool seperation;
    bool flash;
    bool doubleheight;
    bool blink;
    bool hold;
    bool endbox;
    bool startbox;

    char last_ch = ' ';
    char ch;

    uint fgcolor = TTColor::WHITE;
    uint bgcolor = TTColor::BLACK;
    uint newfgcolor = TTColor::WHITE;
    uint newbgcolor = TTColor::BLACK;

    if (m_curpage_issubtitle || m_transparent)
    {
        bgcolor    = TTColor::TRANSPARENT;
        newbgcolor = TTColor::TRANSPARENT;
    }

    SetForegroundColor(fgcolor);
    SetBackgroundColor(bgcolor);

    mosaic = false;
    seperation = false;
    conceal = false;
    flash = false;
    doubleheight = false;
    blink = false;
    hold = false;
    endbox = false;
    startbox = false;

    if (row == 1)
    {
        for (uint x = 0; x < 8; x++)
            DrawBackground(surface, x, 1);
    }

    for (uint x = (row == 1 ? 8 : 0); x < (uint)kTeletextColumns; ++x)
    {
        if (startbox)
        {
            bgcolor = TTColor::BLACK;
            startbox = false;
        }

        if (endbox)
        {
            bgcolor = TTColor::TRANSPARENT;
            endbox = false;
        }

        SetForegroundColor(fgcolor);
        SetBackgroundColor(bgcolor);

        ch = page[x] & 0x7F;
        switch (ch)
        {
            case 0x00 ... 0x07: // alpha + foreground color
                fgcolor = ch & 7;
                mosaic = false;
                conceal = false;
                goto ctrl;
            case 0x08: // flash
                // XXX
                goto ctrl;
            case 0x09: // steady
                flash = false;
                goto ctrl;
            case 0x0a: // end box
                endbox = true;
                goto ctrl;
            case 0x0b: // start box
                if (x < kTeletextColumns - 1 && (page[x + 1] & 0x7F != 0x0b))
                    startbox = true;
                goto ctrl;
            case 0x0c: // normal height
                doubleheight = false;
                goto ctrl;
            case 0x0d: // double height
                doubleheight = row < kTeletextRows-1;
                goto ctrl;
            case 0x10 ... 0x17: // graphics + foreground color
                fgcolor = ch & 7;
                mosaic = true;
                conceal = false;
                goto ctrl;
            case 0x18: // conceal display
                conceal = true;
                goto ctrl;
            case 0x19: // contiguous graphics
                seperation = false;
                goto ctrl;
            case 0x1a: // separate graphics
                seperation = true;
                goto ctrl;
            case 0x1c: // black background
                bgcolor = TTColor::BLACK;
                goto ctrl;
            case 0x1d: // new background
                bgcolor = fgcolor;
                goto ctrl;
            case 0x1e: // hold graphics
                hold = true;
                goto ctrl;
            case 0x1f: // release graphics
                hold = false;
                goto ctrl;
            case 0x0e: // SO (reserved, double width)
            case 0x0f: // SI (reserved, double size)
            case 0x1b: // ESC (reserved)
                ch = ' ';
                break;
            ctrl:
                ch = ' ';
                if (hold && mosaic)
                    ch = last_ch;
            break;

            case 0x80 ... 0x9f: // these aren't used
                ch = ' '; // BAD_CHAR;
                break;
            default:
                if (conceal && !m_revealHidden)
                    ch = ' ';
                break;
        }

        newfgcolor = fgcolor;
        newbgcolor = bgcolor;

        SetForegroundColor(newfgcolor);
        SetBackgroundColor(newbgcolor);
        if ((row != 0) || (x > 7))
        {
            if (m_transparent)
                SetBackgroundColor(TTColor::TRANSPARENT);

            DrawBackground(surface, x, row);
            if (doubleheight && row < (uint)kTeletextRows)
                DrawBackground(surface, x, row + 1);

            if ((mosaic) && (ch < 0x40 || ch > 0x5F))
            {
                SetBackgroundColor(newfgcolor);
                DrawMosaic(surface, x, row, ch, doubleheight);
            }
            else
            {
                char c2 = cvt_char(ch, lang);
                bool dh = doubleheight && row < (uint)kTeletextRows;
                int  rw = (dh) ? row + 1 : row;
                DrawCharacter(surface, x, rw, c2, dh);
            }
        }
    }
}

void OSDTypeTeletext::DrawHeader(OSDSurface *surface,
                                 const unsigned char* page, int lang) const
{
    if (!m_displaying)
        return;

    if (page != NULL)
        DrawLine(surface, page, 1, lang);

    DrawStatus(surface);
}

void OSDTypeTeletext::DrawPage(OSDSurface *surface) const
{
    if (!m_displaying)
        return;

    const TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (!ttpage)
    {
        // no page selected so show the header and a list of available pages
        DrawHeader(surface, NULL, 0);
        return;
    }

    m_cursubpage = ttpage->subpagenum;

    int a = 0;
    if ((ttpage->subtitle) ||
        (ttpage->flags & (TP_SUPPRESS_HEADER | TP_NEWSFLASH | TP_SUBTITLE)))
    {
        a = 1; // when showing subtitles we don't want to see the teletext
               // header line, so we skip that line...
        m_curpage_showheader = false;
        m_curpage_issubtitle = true;
    }
    else
    {
        m_curpage_issubtitle = false;
        m_curpage_showheader = true;
        DrawHeader(surface, m_header, ttpage->lang);

        m_header_changed = false;
    }

    for (int y = kTeletextRows - a; y >= 2; y--)
        DrawLine(surface, ttpage->data[y-1], y, ttpage->lang);

    m_page_changed = false;
}

void OSDTypeTeletext::Reinit(float wmult, float hmult)
{
    QMutexLocker locker(&m_lock);

    m_displayrect = bias(m_unbiasedrect, wmult, hmult);
    m_tt_colspace = m_displayrect.width()  / kTeletextColumns;
    m_tt_rowspace = m_displayrect.height() / kTeletextRows;
}

void OSDTypeTeletext::Draw(OSDSurface *surface,
                           int /*fade*/, int /*maxfade*/,
                           int /*xoff*/, int /*yoff*/)
{
    QMutexLocker locker(&m_lock);

    DrawPage(surface);
}

void OSDTypeTeletext::DrawStatus(OSDSurface *surface) const
{
    SetForegroundColor(TTColor::WHITE);
    SetBackgroundColor(TTColor::BLACK);

    if (!m_transparent)
        for (int i = 0; i < 40; ++i)
            DrawBackground(surface, i, 0);

    DrawCharacter(surface, 1, 0, 'P', 0);
    DrawCharacter(surface, 2, 0, m_pageinput[0], 0);
    DrawCharacter(surface, 3, 0, m_pageinput[1], 0);
    DrawCharacter(surface, 4, 0, m_pageinput[2], 0);

    const TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (!ttpage)
    {
        SetBackgroundColor(TTColor::BLACK);
        SetForegroundColor(TTColor::WHITE);

        if (!m_transparent)
            for (int i = 7; i < 40; i++)
                DrawBackground(surface, i, 0);

        QString str = QObject::tr("Page Not Available",
                                  "Requested Teletext page not available");
        for (uint i = 0; (i < 30) && i < str.length(); i++)
            DrawCharacter(surface, i+10, 0, str[i], 0);

        return;
    }

    // get list of available sub pages
    QString str = "";
    int count = 1, selected = 0;
    const TeletextPage *page = FindPage(m_curpage);
    if (page)
    {
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
    }

    if (str.isEmpty())
        return;

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

    SetForegroundColor(TTColor::WHITE);
    for (int x = 0; x < 11; x++)
    {
        if (m_transparent)
            SetBackgroundColor(TTColor::TRANSPARENT);
        else
            SetBackgroundColor(TTColor::BLACK);

        DrawBackground(surface, x * 3 + 7, 0);

        if (str[x * 3] == '*')
        {
            str[x * 3] = ' ';
            SetBackgroundColor(TTColor::RED);
        }

        DrawBackground(surface, x * 3 + 8, 0);
        DrawBackground(surface, x * 3 + 9, 0);

        DrawCharacter(surface, x * 3 + 7, 0, str[x * 3], 0);
        DrawCharacter(surface, x * 3 + 8, 0, str[x * 3 + 1], 0);
        DrawCharacter(surface, x * 3 + 9, 0, str[x * 3 + 2], 0);
    }
}

