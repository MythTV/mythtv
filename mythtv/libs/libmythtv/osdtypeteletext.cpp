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

//#include <iostream>
//using namespace std;

#include <qcolor.h>
#include <qapplication.h>
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
                                 QRect displayrect, float wmult, float hmult)
    : OSDType(name),
      m_displayrect(displayrect),       m_unbiasedrect(0,0,0,0),
      m_font(font),                     m_surface(NULL),
      m_box(NULL),

      m_tt_colspace(m_displayrect.width()  / kTeletextColumns),
      m_tt_rowspace(m_displayrect.height() / kTeletextRows),

      m_bgcolor_y(0),                   m_bgcolor_u(0),
      m_bgcolor_v(0),                   m_bgcolor_a(0),
      // last fetched page
      m_fetchpage(0),                   m_fetchsubpage(0),
      // currently displayed page:
      m_curpage(0x100),                 m_cursubpage(-1),
      m_curpage_showheader(true),       m_curpage_issubtitle(false),

      m_transparent(false),             m_revealHidden(false),
      m_displaying(false)
{
    m_unbiasedrect  = bias(m_displayrect, wmult, hmult);

    m_pageinput[0] = '1';
    m_pageinput[1] = '0';
    m_pageinput[2] = '0';

    // fill Bitswap
    for (int i = 0; i < 256; i++)
    {
        bitswap[i] = 0;
        for (int bit = 0; bit < 8; bit++)
            if (i & (1 << bit))
                bitswap[i] |= (1 << (7-bit));
    }

    Reset(); // initializes m_magazines
}

OSDTypeTeletext::OSDTypeTeletext(const OSDTypeTeletext &other)
    : OSDType(other.m_name)
{
}

OSDTypeTeletext::~OSDTypeTeletext()
{
}

/** \fn OSDTypeTeletext::Reset(void)
 *  \brief Resets the teletext data cache
 */
void OSDTypeTeletext::Reset(void)
{
    for (uint mag = 0; mag < 8; mag++)
    {
        QMutexLocker lock(&m_magazines[mag].lock);

        // clear all sub pages in page
        std::map<int, TeletextPage>::iterator iter;
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
    }

    m_curpage    = 0x100;
    m_cursubpage = -1;
    m_curpage_showheader = true;
}

/** \fn OSDTypeTeletext::CharConversion(char, int)
 *  \brief converts the given character to the given language
 */
char OSDTypeTeletext::CharConversion(char ch, int lang)
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

/** \fn OSDTypeTeletext::AddPageHeader(int,int,const unsigned char*,int,int,int)
 *  \brief Adds a new page header 
 *         (page=0 if none)
 */
void OSDTypeTeletext::AddPageHeader(int page, int subpage,
                                    const unsigned char* buf,
                                    int vbimode, int lang, int flags)
{
    if (page < 256)
        return;

    int magazine = MAGAZINE(page);
    int lastPage = m_magazines[magazine - 1].current_page;
    int lastSubPage = m_magazines[magazine - 1].current_subpage;

    // update the last fetched page if the magazine is the same 
    // and the page no. is different
    if (page != lastPage || subpage != lastSubPage)
        PageUpdated(lastPage, lastSubPage);

    m_fetchpage = page;
    m_fetchsubpage = subpage;

    TeletextSubPage *ttpage = FindSubPage(page, subpage);

    if (ttpage == NULL)
    {
        ttpage = &m_magazines[magazine - 1].pages[page].subpages[subpage];
        m_magazines[magazine - 1].pages[page].pagenum = page;
        ttpage->subpagenum = subpage;

        for (int i=0; i < 6; i++)
            ttpage->floflink[i] = 0;
    }

    m_magazines[magazine - 1].current_page = page;
    m_magazines[magazine - 1].current_subpage = subpage;

    ttpage->lang = lang;
    ttpage->flags = flags;
    ttpage->flof = 0;

    ttpage->subtitle = (vbimode == VBI_DVB_SUBTITLE);

    for (int j = 0; j < 8; j++)
        ttpage->data[0][j] = 32;

    if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
    {
        for (int j = 8; j < 40; j++)
            ttpage->data[0][j] = bitswap[buf[j]];
    }
    else 
    {
        memcpy(ttpage->data[0]+0, buf, 40);
        memset(ttpage->data[0]+40, ' ', sizeof(ttpage->data)-40);
    }
    
    if ( !(ttpage->flags & TP_INTERRUPTED_SEQ)) 
        HeaderUpdated(ttpage->data[0],ttpage->lang);
}

/** \fn OSDTypeTeletext::AddTeletextData(int,int,const unsigned char*,int)
 *  \brief Adds Teletext Data from TeletextDecoder
 */
void OSDTypeTeletext::AddTeletextData(int magazine, int row, 
                                      const unsigned char* buf, int vbimode)
{
    int b1, b2, b3, err;

    if (magazine < 1 || magazine > 8)
        return;

    int currentpage = m_magazines[magazine - 1].current_page;
    int currentsubpage = m_magazines[magazine -1].current_subpage;
    if (currentpage == 0)
        return;

    TeletextSubPage *ttpage = FindSubPage(currentpage, currentsubpage);

    if (ttpage == NULL)
        return;

    switch (row)
    {
        case 1 ... 24: // Page Data
            if (vbimode == VBI_DVB || vbimode == VBI_DVB_SUBTITLE)
            {
                for (int j = 0; j < 40; j++)
                ttpage->data[row][j] = bitswap[buf[j]];
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

/** \fn OSDTypeTeletext::PageUpdated(int)
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


    if (m_surface != NULL)
    {
        m_surface->SetChanged(true);
        m_surface->ClearUsed();
        DrawPage();
    }
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
    if (!m_displaying)
        return;

    if (page == NULL)
        return;

    if (m_curpage_showheader == false)
        return;

    if (m_surface != NULL)
        DrawHeader(page, lang);
}

/** \fn OSDTypeTeletext::FindPage(int, int)
 *  \brief Finds the given page
 *
 *  \param page Page number
 *  \param direction find page before or after the given page
 *  \return TeletextPage (NULL if not found) 
 */
TeletextPage* OSDTypeTeletext::FindPage(int page, int direction)
{
    TeletextPage *res = NULL;
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return NULL;

    QMutexLocker lock(&m_magazines[mag - 1].lock);

    std::map<int, TeletextPage>::iterator pageIter;
    pageIter = m_magazines[mag - 1].pages.find(page);
    if (pageIter == m_magazines[mag - 1].pages.end())
        return NULL;

    res = &pageIter->second;
    if (direction == -1)
    {
        --pageIter;
        if (pageIter == m_magazines[mag - 1].pages.end())
        {
            std::map<int, TeletextPage>::reverse_iterator iter;
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

/** \fn OSDTypeTeletext::FindSubPage(int, int, int)
 *  \brief Finds the given page
 *
 *  \param page Page number
 *  \param subpage Subpage number (if set to -1, find the first subpage)
 *  \param direction find page before or after the given page
 *         (only if Subpage is not -1)
 *  \return TeletextSubPage (NULL if not found) 
 */
TeletextSubPage* OSDTypeTeletext::FindSubPage(int page, int subpage, int direction)
{
    TeletextSubPage *res = NULL;
    int mag = MAGAZINE(page);

    if (mag > 8 || mag < 1)
        return NULL;

    QMutexLocker lock(&m_magazines[mag - 1].lock);

    if (subpage == -1)
    {
        // return the first subpage found
        std::map<int, TeletextPage>::iterator pageIter;
        pageIter = m_magazines[mag - 1].pages.find(page);
        if (pageIter == m_magazines[mag - 1].pages.end())
            return NULL;

        TeletextPage *page = &pageIter->second;
        std::map<int, TeletextSubPage>::iterator subpageIter;
        subpageIter = page->subpages.begin();
        if (subpageIter == page->subpages.end())
            return NULL;

        return &subpageIter->second;
    }
    else
    {
        // try to find the subpage given
        std::map<int, TeletextPage>::iterator pageIter;
        pageIter = m_magazines[mag - 1].pages.find(page);
        if (pageIter == m_magazines[mag - 1].pages.end())
            return NULL;

        TeletextPage *page = &pageIter->second;
        std::map<int, TeletextSubPage>::iterator subpageIter;
        subpageIter = page->subpages.find(subpage);
        if (subpageIter == page->subpages.end())
            return NULL;

        res = &subpageIter->second;
        if (direction == -1)
        {
            --subpageIter;
            if (subpageIter == page->subpages.end())
            {
                std::map<int, TeletextSubPage>::reverse_iterator iter;
                iter = page->subpages.rbegin();
                res = &iter->second;
            }
            else
                res = &subpageIter->second;
        }

        if (direction == 1)
        {
            ++subpageIter;
            if (subpageIter == page->subpages.end())
            {
                subpageIter = page->subpages.begin();
                res = &subpageIter->second;
            }
            else
                res = &subpageIter->second;
        }
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

/** \fn OSDTypeTeletext::SetForegroundColor(int)
 *  \brief Set the font color to the given color.
 *
 *   NOTE: TTColor::TRANSPARENT does not do anything!
 *  \sa TTColor
 */
void OSDTypeTeletext::SetForegroundColor(int color)
{
    switch (color & ~TTColor::TRANSPARENT)
    {
        case TTColor::BLACK:   m_font->setColor(kColorBlack);   break;
        case TTColor::RED:     m_font->setColor(kColorRed);     break;
        case TTColor::GREEN:   m_font->setColor(kColorGreen);   break;
        case TTColor::YELLOW:  m_font->setColor(kColorYellow);  break;
        case TTColor::BLUE:    m_font->setColor(kColorBlue);    break;
        case TTColor::MAGENTA: m_font->setColor(kColorMagenta); break;
        case TTColor::CYAN:    m_font->setColor(kColorCyan);    break;
        case TTColor::WHITE:   m_font->setColor(kColorWhite);   break;
    }

    m_font->setShadow(0,0);
    m_font->setOutline(0);
}

/** \fn OSDTypeTeletext:SetBackgroundColor(int)
 *  \brief Set the background color to the given color
 *
 *  \sa TTColor
 */
void OSDTypeTeletext::SetBackgroundColor(int ttcolor)
{
    QColor color;

    switch (ttcolor & ~TTColor::TRANSPARENT)
    {
        case TTColor::BLACK:   color = kColorBlack;   break;
        case TTColor::RED:     color = kColorRed;     break;
        case TTColor::GREEN:   color = kColorGreen;   break;
        case TTColor::YELLOW:  color = kColorYellow;  break;
        case TTColor::BLUE:    color = kColorBlue;    break;
        case TTColor::MAGENTA: color = kColorMagenta; break;
        case TTColor::CYAN:    color = kColorCyan;    break;
        case TTColor::WHITE:   color = kColorWhite;   break;
    }

    int r = color.red();
    int g = color.green();
    int b = color.blue();

    float y = (0.299*r) + (0.587*g) + (0.114*b);
    float u = (0.564*(b - y)); // = -0.169R-0.331G+0.500B
    float v = (0.713*(r - y)); // = 0.500R-0.419G-0.081B

    m_bgcolor_y = (uint8_t)(y);
    m_bgcolor_u = (uint8_t)(127 + u);
    m_bgcolor_v = (uint8_t)(127 + v);
    m_bgcolor_a = (ttcolor & TTColor::TRANSPARENT) ? 0x00 : 0xff;
}

/** \fn OSDTypeTeletext::DrawBackground(int, int)
 *  \brief draws background in the color set in setBackgroundColor
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 */
void OSDTypeTeletext::DrawBackground(int x, int y)
{
    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    DrawRect(x, y, m_tt_colspace, m_tt_rowspace);
}

/** \fn OSDTypeteletext::drawRect(int, int, int, int)
 *  \brief draws a Rectangle at position x and y (width dx, height dy)
 *         with the color set in setBackgroundColor
 *
 *  \param x X position at the screen
 *  \param y Y position at the screen
 *  \param dx width
 *  \param dy height
 */
void OSDTypeTeletext::DrawRect(int x, int y, int dx, int dy)
{
    QRect all(x, y, dx, dy);
    m_surface->AddRect(all);

    int luma_stride = m_surface->width;
    int chroma_stride = m_surface->width >> 1;
    int ye = y + dy;

    unsigned char *buf_y = m_surface->y + (luma_stride * y) + x;
    unsigned char *buf_u = m_surface->u + (chroma_stride * (y>>1)) + (x>>1);
    unsigned char *buf_v = m_surface->v + (chroma_stride * (y>>1)) + (x>>1);
    unsigned char *buf_a = m_surface->alpha + (luma_stride * y) + x;

    for (; y<ye; ++y)
    {
        for (int i=0; i<dx; ++i)
        {
            buf_y[i] = m_bgcolor_y;
            buf_a[i] = m_bgcolor_a;
        }

        if ((y & 1) == 0)
        {
            for (int i=0; i<dx; ++i)
            {
                buf_u[i>>1] = m_bgcolor_u;
                buf_v[i>>1] = m_bgcolor_v;
            }

            buf_u += chroma_stride;
            buf_v += chroma_stride;
        }

        buf_y += luma_stride;
        buf_a += luma_stride;
    }
}

/** \fn OSDTypeTeletext::DrawCharacter(int, int, QChar, int)
 *  \brief Draws a character at posistion x, y 
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 *  \param ch Character
 *  \param doubleheight if different to 0, draw doubleheighted character
 */
void OSDTypeTeletext::DrawCharacter(int x, int y, QChar ch, int doubleheight)
{
    if (!m_font)
        return;

    QString line = ch;

    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    m_font->DrawString(m_surface, x, y, line,
                       m_surface->width, m_surface->height,
                       255, doubleheight!=0);
}

/** \fn OSDTypeTeletext::DrawMosaic(int, int, code, int)
 *  \brief Draws a mosaic as defined in ETSI EN 300 706
 *
 *  \param x X position (40 cols)
 *  \param y Y position (25 rows)
 *  \param code Code
 *  \param doubleheight if different to 0, draw doubleheighted mosaic)
 */
void OSDTypeTeletext::DrawMosaic(int x, int y, int code, int doubleheight)
{
    (void)x;
    (void)y;
    (void)code;

    x *= m_tt_colspace;
    x += m_displayrect.left();

    y *= m_tt_rowspace;
    y += m_displayrect.top();

    int dx = (int)round(m_tt_colspace / 2)+1;
    int dy = (int)round(m_tt_rowspace / 3)+1;

    if (doubleheight != 0)
    {
        dx *= 2;
        dy *= 2;
    }

    if (code & 0x10) DrawRect(x,      y + 2*dy, dx, dy);
    if (code & 0x40) DrawRect(x + dx, y + 2*dy, dx, dy);
    if (code & 0x01) DrawRect(x,      y,        dx, dy);
    if (code & 0x02) DrawRect(x + dx, y,        dx, dy);
    if (code & 0x04) DrawRect(x,      y + dy,   dx, dy);
    if (code & 0x08) DrawRect(x + dx, y + dy,   dx, dy);
}

void OSDTypeTeletext::DrawLine(const unsigned char* page, uint row, int lang)
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
            DrawBackground(x, 1);
    }

    for (uint x = (row == 1 ? 8 : 0); x < (uint)kTeletextColumns; ++x)
    {
        if (startbox)
        {
            bgcolor = bgcolor = TTColor::BLACK;
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
                doubleheight = 0;
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

            DrawBackground(x, row);
            if (doubleheight != 0)
                    DrawBackground(x, row +1);

            if ((mosaic) && (ch < 0x40 || ch > 0x5F))
            {
                SetBackgroundColor(newfgcolor);
                DrawMosaic(x, row, ch, doubleheight);
            }
            else 
            {
                if (doubleheight != 0)
                    DrawCharacter(x, row+1, CharConversion(ch, lang),
                                  doubleheight);
                else
                    DrawCharacter(x, row, CharConversion(ch, lang),
                                  doubleheight);
            }
        }
    }
}

void OSDTypeTeletext::DrawHeader(const unsigned char* page, int lang)
{
    if (!m_displaying)
        return;

    DrawLine(page, 1, lang);
    StatusUpdated();
}

void OSDTypeTeletext::DrawPage(void)
{
    if (!m_displaying)
        return;

    TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (ttpage == NULL)
        return;

    m_cursubpage = ttpage->subpagenum;

    int a = 0;
    if ((ttpage->subtitle) || (ttpage->flags & 
            (TP_SUPPRESS_HEADER + TP_NEWSFLASH + TP_SUBTITLE)))
    {
        a = 1;
        m_curpage_showheader = false;
        m_curpage_issubtitle = true;
    }
    else
    {
        m_curpage_issubtitle = false;
        m_curpage_showheader = true;
        DrawHeader(ttpage->data[0], ttpage->lang);
    }

    for (int y = kTeletextRows-a; y >= 2; y--)
        DrawLine(ttpage->data[y-1], y, ttpage->lang);
}

void OSDTypeTeletext::Reinit(float wmult, float hmult)
{
    m_displayrect = bias(m_unbiasedrect, wmult, hmult);
    m_tt_colspace = m_displayrect.width()  / kTeletextColumns;
    m_tt_rowspace = m_displayrect.height() / kTeletextRows;
}

void OSDTypeTeletext::Draw(OSDSurface *surface, int fade, int maxfade,
                           int xoff, int yoff)
{
    (void)surface;
    (void)fade;
    (void)maxfade;
    (void)xoff;
    (void)yoff;

    m_surface = surface;
    DrawPage();
}

void OSDTypeTeletext::StatusUpdated(void)
{
    SetForegroundColor(TTColor::WHITE);
    SetBackgroundColor(TTColor::BLACK);

    if (!m_transparent)
        for (int i = 0; i < 40; ++i)
            DrawBackground(i, 0);

    DrawCharacter(1, 0, 'P', 0);
    DrawCharacter(2, 0, m_pageinput[0], 0);
    DrawCharacter(3, 0, m_pageinput[1], 0);
    DrawCharacter(4, 0, m_pageinput[2], 0);

    TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (!ttpage)
    {
        SetBackgroundColor(TTColor::BLACK);
        SetForegroundColor(TTColor::WHITE);

        if (!m_transparent)
            for (int i = 7; i < 40; i++)
                DrawBackground(i, 0);

        char *str = "Page Not Available";
        for (unsigned int i = 0; i < strlen(str); ++i)
            DrawCharacter(i+10, 0, str[i], 0);

        return;
    }

    // get list of available sub pages
    QString str = "";
    int count = 1, selected = 0;
    TeletextPage *page = FindPage(m_curpage);
    if (page)
    {
        std::map<int, TeletextSubPage>::iterator subpageIter;
        subpageIter = page->subpages.begin();
        while (subpageIter != page->subpages.end())
        {
            TeletextSubPage *subpage = &subpageIter->second;

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

        DrawBackground(x * 3 + 7, 0);

        if (str[x * 3] == '*')
        {
            str[x * 3] = ' ';
            SetBackgroundColor(TTColor::RED);
        }

        DrawBackground(x * 3 + 8, 0);
        DrawBackground(x * 3 + 9, 0);

        DrawCharacter(x * 3 + 7, 0, str[x * 3], 0);
        DrawCharacter(x * 3 + 8, 0, str[x * 3 + 1], 0);
        DrawCharacter(x * 3 + 9, 0, str[x * 3 + 2], 0);
    }
}

