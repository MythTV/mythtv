#include <QFontMetrics>

#include "mythverbose.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythuishape.h"
#include "vbilut.h"
#include "teletextscreen.h"

#define LOC QString("Teletext: ")
#define MAGAZINE(page) (page / 256)

const QColor TeletextScreen::kColorBlack      = QColor(  0,  0,  0,255);
const QColor TeletextScreen::kColorRed        = QColor(255,  0,  0,255);
const QColor TeletextScreen::kColorGreen      = QColor(  0,255,  0,255);
const QColor TeletextScreen::kColorYellow     = QColor(255,255,  0,255);
const QColor TeletextScreen::kColorBlue       = QColor(  0,  0,255,255);
const QColor TeletextScreen::kColorMagenta    = QColor(255,  0,255,255);
const QColor TeletextScreen::kColorCyan       = QColor(  0,255,255,255);
const QColor TeletextScreen::kColorWhite      = QColor(255,255,255,255);
const int    TeletextScreen::kTeletextColumns = 40;
const int    TeletextScreen::kTeletextRows    = 26;

static MythFontProperties* gTTFont;

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

TeletextScreen::TeletextScreen(MythPlayer *player, const char * name) :
    MythScreenType((MythScreenType*)NULL, name),
    m_player(player),           m_safeArea(QRect()),
    m_colSize(10),              m_rowSize(10),
    m_fetchpage(0),             m_fetchsubpage(0),
    m_bgColor(QColor(kColorBlack)),
    m_curpage(0x100),           m_cursubpage(-1),
    m_curpage_showheader(true), m_curpage_issubtitle(false),
    m_transparent(false),       m_revealHidden(false),
    m_displaying(false),        m_header_changed(false),
    m_page_changed(false)
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

TeletextScreen::~TeletextScreen()
{
}

bool TeletextScreen::Create(void)
{
    return m_player;
}

void TeletextScreen::OptimiseDisplayedArea(void)
{
    QRegion visible;
    QListIterator<MythUIType *> i(m_ChildrenList);
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        visible = visible.united(img->GetArea());
    }

    if (visible.isEmpty())
        return;

    QRect bounding  = visible.boundingRect();
    bounding = bounding.translated(m_safeArea.topLeft());
    bounding = m_safeArea.intersected(bounding);
    int left = m_safeArea.left() - bounding.left();
    int top  = m_safeArea.top()  - bounding.top();
    SetArea(MythRect(bounding));

    i.toFront();;
    while (i.hasNext())
    {
        MythUIType *img = i.next();
        img->SetArea(img->GetArea().translated(left, top));
    }
}

void TeletextScreen::Pulse(void)
{
    if (!InitialiseFont() || !m_displaying)
        return;

    if (m_player && m_player->getVideoOutput())
    {
        QRect oldsafe = m_safeArea;
        m_safeArea = m_player->getVideoOutput()->GetSafeRect();
        if (oldsafe != m_safeArea)
            m_page_changed = true;
        m_colSize = (int)((float)m_safeArea.width() / (float)kTeletextColumns);
        m_rowSize = (int)((float)m_safeArea.height() / (float)kTeletextRows);
        gTTFont->GetFace()->setPixelSize(m_safeArea.height() /
                                        (kTeletextRows * 1.2));
    }
    else
    {
        return;
    }

    if (!m_page_changed)
        return;

    QMutexLocker locker(&m_lock);

    DeleteAllChildren();

    const TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (!ttpage)
    {
        // no page selected so show the header and a list of available pages
        DrawHeader(NULL, 0);
        m_page_changed = false;
        OptimiseDisplayedArea();
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
        DrawHeader(m_header, ttpage->lang);

        m_header_changed = false;
    }

    for (int y = kTeletextRows - a; y >= 2; y--)
        DrawLine(ttpage->data[y-1], y, ttpage->lang);

    m_page_changed = false;
    OptimiseDisplayedArea();
}

void TeletextScreen::KeyPress(uint key)
{
    if (!m_displaying)
        return;

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

            PageUpdated(m_curpage, m_cursubpage);
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

void TeletextScreen::SetPage(int page, int subpage)
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

void TeletextScreen::SetDisplaying(bool display)
{
    m_displaying = display;
    if (!m_displaying)
        DeleteAllChildren();
}

void TeletextScreen::Reset(void)
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

void TeletextScreen::AddPageHeader(int page, int subpage,
                                    const uint8_t * buf,
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

/**
 *  \brief Adds Teletext Data from TeletextDecoder
 */
void TeletextScreen::AddTeletextData(int magazine, int row,
                                      const uint8_t * buf, int vbimode)
{
    QMutexLocker locker(&m_lock);

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

void TeletextScreen::DrawHeader(const uint8_t *page, int lang)
{
    if (!m_displaying)
        return;

    if (page != NULL)
        DrawLine(page, 1, lang);

    DrawStatus();
}

QColor ttcolortoqcolor(int ttcolor)
{
    QColor color;

    switch (ttcolor & ~kTTColorTransparent)
    {
        case kTTColorBlack:   color = TeletextScreen::kColorBlack;   break;
        case kTTColorRed:     color = TeletextScreen::kColorRed;     break;
        case kTTColorGreen:   color = TeletextScreen::kColorGreen;   break;
        case kTTColorYellow:  color = TeletextScreen::kColorYellow;  break;
        case kTTColorBlue:    color = TeletextScreen::kColorBlue;    break;
        case kTTColorMagenta: color = TeletextScreen::kColorMagenta; break;
        case kTTColorCyan:    color = TeletextScreen::kColorCyan;    break;
        case kTTColorWhite:   color = TeletextScreen::kColorWhite;   break;
    }

    return color;
}

static QString TTColorToString(int ttcolor)
{
    switch (ttcolor & ~kTTColorTransparent)
    {
        case kTTColorBlack:   return "Black";
        case kTTColorRed:     return "Red";
        case kTTColorGreen:   return "Green";
        case kTTColorYellow:  return "Yellow";
        case kTTColorBlue:    return "Blue";
        case kTTColorMagenta: return "Magenta";
        case kTTColorCyan:    return "Cyan";
        case kTTColorWhite:   return "White";
        default:              return "Unknown";
    }
}

void TeletextScreen::SetForegroundColor(int ttcolor)
{
    VERBOSE(VB_VBI|VB_EXTRA, QString("SetForegroundColor(%1)")
            .arg(TTColorToString(ttcolor)));

    gTTFont->SetColor(ttcolortoqcolor(ttcolor));
}

void TeletextScreen::SetBackgroundColor(int ttcolor)
{
    VERBOSE(VB_VBI|VB_EXTRA, QString("SetBackgroundColor(%1)")
            .arg(TTColorToString(ttcolor)));

    m_bgColor = ttcolortoqcolor(ttcolor);
    m_bgColor.setAlpha((ttcolor & kTTColorTransparent) ? 0x00 : 0xff);
}

void TeletextScreen::DrawLine(const uint8_t *page, uint row, int lang)
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

    unsigned char last_ch = ' ';
    unsigned char ch;

    uint fgcolor    = kTTColorWhite;
    uint bgcolor    = kTTColorBlack;
    uint newfgcolor = kTTColorWhite;
    uint newbgcolor = kTTColorBlack;

    if (m_curpage_issubtitle || m_transparent)
    {
        bgcolor    = kTTColorTransparent;
        newbgcolor = kTTColorTransparent;

        bool isBlank = true;
        for (uint i = (row == 1 ? 8 : 0); i < (uint) kTeletextColumns; i++)
        {
            ch = page[i] & 0x7F;
            if (ch != ' ')
            {
                isBlank = false;
                break;
            }
        }

        if (isBlank)
            return;
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
    uint flof_link_count = 0;
    uint old_bgcolor = bgcolor;

    if (row == 1)
    {
        for (uint x = 0; x < 8; x++)
            DrawBackground(x, 1);
    }

    for (uint x = (row == 1 ? 8 : 0); x < (uint)kTeletextColumns; ++x)
    {
        if (startbox)
        {
            old_bgcolor = bgcolor;
            if (kTTColorTransparent & bgcolor)
                bgcolor = bgcolor & ~kTTColorTransparent;
            startbox = false;
        }

        if (endbox)
        {
            bgcolor = old_bgcolor;
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
                // increment FLOF/FastText count if menu item detected
                flof_link_count += (row == 25) ? 1 : 0;
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
                if (x < kTeletextColumns - 1 && ((page[x + 1] & 0x7F) != 0x0b))
                    startbox = true;
                goto ctrl;
            case 0x0c: // normal height
                doubleheight = false;
                goto ctrl;
            case 0x0d: // double height
                doubleheight = (row < (kTeletextRows-1)) && (x < (kTeletextColumns - 1));
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
                bgcolor = kTTColorBlack;
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

        // Hide FastText/FLOF menu characters if not available
        if (flof_link_count && (flof_link_count <= 6))
        {
            const TeletextSubPage *ttpage =
                FindSubPage(m_curpage, m_cursubpage);

            if (ttpage)
            {
                bool has_flof = ttpage->floflink[flof_link_count - 1];
                ch = (has_flof) ? ch : ' ';
            }
        }

        newfgcolor = fgcolor;
        newbgcolor = bgcolor;

        SetForegroundColor(newfgcolor);
        SetBackgroundColor(newbgcolor);
        if ((row != 0) || (x > 7))
        {
            if (m_transparent)
                SetBackgroundColor(kTTColorTransparent);

            DrawBackground(x, row);
            if (doubleheight && row < (uint)kTeletextRows)
                DrawBackground(x, row + 1);

            if ((mosaic) && (ch < 0x40 || ch > 0x5F))
            {
                SetBackgroundColor(newfgcolor);
                DrawMosaic(x, row, ch, doubleheight);
            }
            else
            {
                char c2 = cvt_char(ch, lang);
                DrawCharacter(x, row, c2, doubleheight);
            }
        }
    }
}

void TeletextScreen::DrawCharacter(int x, int y, QChar ch, int doubleheight)
{
    QString line = ch;

    x *= m_colSize;
    y *= m_rowSize;
    int height = m_rowSize * (doubleheight ? 2 : 1);
    QRect rect(x, y, m_colSize, height);

    int fontheight = 10;
    if (doubleheight)
    {
        fontheight = m_safeArea.height() / (kTeletextRows * 1.2);
        int doubleheight = fontheight * 2;
        gTTFont->GetFace()->setPixelSize(doubleheight);
        gTTFont->GetFace()->setStretch(50);
    }

    QString name = QString("ttchar%1%2%3").arg(x).arg(y).arg(line);
    MythUIText *txt = new MythUIText(line, *gTTFont, rect,
                                     rect, this, name);
    if (txt)
        txt->SetJustification(Qt::AlignCenter);

    if (doubleheight)
    {
        gTTFont->GetFace()->setPixelSize(fontheight);
        gTTFont->GetFace()->setStretch(100);
    }
}

void TeletextScreen::DrawBackground(int x, int y)
{
    x *= m_colSize;
    y *= m_rowSize;
    DrawRect(QRect(x, y, m_colSize, m_rowSize));
}

void TeletextScreen::DrawRect(QRect rect)
{
    QString name = QString("ttbg%1%2%3%4")
        .arg(rect.left()).arg(rect.top()).arg(rect.width()).arg(rect.height());
    QBrush bgfill = QBrush(m_bgColor, Qt::SolidPattern);
    MythUIShape *shape = new MythUIShape(this, name);
    shape->SetArea(MythRect(rect));
    shape->SetFillBrush(bgfill);
}

void TeletextScreen::DrawMosaic(int x, int y, int code, int doubleheight)
{
    x *= m_colSize;
    y *= m_rowSize;

    int dx = (int)round(m_colSize / 2) + 1;
    int dy = (int)round(m_rowSize / 3) + 1;
    dy = (doubleheight) ? (2 * dy) : dy;

    if (code & 0x10)
        DrawRect(QRect(x,      y + 2*dy, dx, dy));
    if (code & 0x40)
        DrawRect(QRect(x + dx, y + 2*dy, dx, dy));
    if (code & 0x01)
        DrawRect(QRect(x,      y,        dx, dy));
    if (code & 0x02)
        DrawRect(QRect(x + dx, y,        dx, dy));
    if (code & 0x04)
        DrawRect(QRect(x,      y + dy,   dx, dy));
    if (code & 0x08)
        DrawRect(QRect(x + dx, y + dy,   dx, dy));
}

void TeletextScreen::DrawStatus(void)
{
    SetForegroundColor(kTTColorWhite);
    SetBackgroundColor(kTTColorBlack);

    if (!m_transparent)
        for (int i = 0; i < 40; ++i)
            DrawBackground(i, 0);

    DrawCharacter(1, 0, 'P', 0);
    DrawCharacter(2, 0, m_pageinput[0], 0);
    DrawCharacter(3, 0, m_pageinput[1], 0);
    DrawCharacter(4, 0, m_pageinput[2], 0);

    const TeletextSubPage *ttpage = FindSubPage(m_curpage, m_cursubpage);

    if (!ttpage)
    {
        SetBackgroundColor(kTTColorBlack);
        SetForegroundColor(kTTColorWhite);

        if (!m_transparent)
            for (int i = 7; i < 40; i++)
                DrawBackground(i, 0);

        QString str = QObject::tr("Page Not Available",
                                  "Requested Teletext page not available");
        for (int i = 0; (i < 30) && i < str.length(); i++)
            DrawCharacter(i+10, 0, str[i], 0);

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

    SetForegroundColor(kTTColorWhite);
    for (int x = 0; x < 11; x++)
    {
        if (m_transparent)
            SetBackgroundColor(kTTColorTransparent);
        else
            SetBackgroundColor(kTTColorBlack);

        DrawBackground(x * 3 + 7, 0);

        if (str[x * 3] == '*')
        {
            str[x * 3] = ' ';
            SetBackgroundColor(kTTColorRed);
        }

        DrawBackground(x * 3 + 8, 0);
        DrawBackground(x * 3 + 9, 0);

        DrawCharacter(x * 3 + 7, 0, str[x * 3], 0);
        DrawCharacter(x * 3 + 8, 0, str[x * 3 + 1], 0);
        DrawCharacter(x * 3 + 9, 0, str[x * 3 + 2], 0);
    }
}

void TeletextScreen::PageUpdated(int page, int subpage)
{
    if (!m_displaying)
        return;
    if (page != m_curpage)
        return;
    if (subpage != m_cursubpage && m_cursubpage != -1)
        return;
    m_page_changed = true;
}

void TeletextScreen::HeaderUpdated(uint8_t * page, int lang)
{
    (void)lang;

    if (!m_displaying)
        return;

    if (page == NULL)
        return;

    if (m_curpage_showheader == false)
        return;

    m_header_changed = true;
}

const TeletextPage *TeletextScreen::FindPageInternal(
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

const TeletextSubPage *TeletextScreen::FindSubPageInternal(
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

bool TeletextScreen::InitialiseFont(void)
{
    static bool initialised = false;
    if (initialised)
        return gTTFont;

    QString font = gCoreContext->GetSetting("OSDCCFont");

    MythFontProperties *mythfont = new MythFontProperties();
    if (mythfont)
    {
        QFont newfont(font);
        font.detach();
        mythfont->SetFace(newfont);
        gTTFont = mythfont;
    }
    else
        return false;

    initialised = true;
    VERBOSE(VB_PLAYBACK, LOC + QString("Loaded Teletext font"));
    return true;
}
