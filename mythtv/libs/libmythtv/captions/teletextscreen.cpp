#include <algorithm>

#include <QFontMetrics>
#include <QPainter>

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythfontproperties.h"
#include "libmythui/mythimage.h"
#include "libmythui/mythpainter.h"
#include "libmythui/mythuiimage.h"
#include "libmythui/mythuishape.h"
#include "libmythui/mythuitext.h"

#include "captions/subtitlescreen.h"
#include "captions/teletextscreen.h"
#include "vbilut.h"

#define LOC QString("TeletextScreen: ")

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
static int gTTBackgroundAlpha;

static QChar cvt_char(char ch, int lang)
{
    for (int j = 0; j < 14; j++)
    {
        int c = ch & 0x7F;
        if (c == lang_chars[0][j])
            ch = lang_chars[lang + 1][j];
    }
    return QLatin1Char(ch);
}

TeletextScreen::TeletextScreen(MythPlayer* Player, MythPainter *Painter, const QString& Name, int FontStretch)
  : MythScreenType(static_cast<MythScreenType*>(nullptr), Name),
    m_player(Player),
    m_fontStretch(FontStretch)
{
    m_painter = Painter;
}

TeletextScreen::~TeletextScreen()
{
    ClearScreen();
}

bool TeletextScreen::Create()
{
    if (m_player)
        m_teletextReader = m_player->GetTeletextReader();
    return m_player && m_teletextReader;
}

void TeletextScreen::ClearScreen()
{
    DeleteAllChildren();
    for (const auto & img : std::as_const(m_rowImages))
        delete img;
    m_rowImages.clear();
    SetRedraw();
}

QImage* TeletextScreen::GetRowImage(int row, QRect &rect)
{
    int y   = row & ~1;
    rect.translate(0, -(y * m_rowHeight));
    if (!m_rowImages.contains(y))
    {
        auto* img = new QImage(m_safeArea.width(), m_rowHeight * 2,
                                 QImage::Format_ARGB32);
        if (img)
        {
            img->fill(0);
            m_rowImages.insert(y, img);
        }
        else
        {
            return nullptr;
        }
    }
    return m_rowImages.value(y);
}

void TeletextScreen::OptimiseDisplayedArea()
{
    QHashIterator<int, QImage*> it(m_rowImages);
    while (it.hasNext())
    {
        it.next();
        MythImage *image = m_painter->GetFormatImage();
        if (!image || !it.value())
            continue;

        int row = it.key();
        image->Assign(*(it.value()));
        auto *uiimage = new MythUIImage(this, QString("ttrow%1").arg(row));
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(0, row * m_rowHeight, m_safeArea.width(), m_rowHeight * 2));
        }
        image->DecrRef();
    }

    QRegion visible;
    QListIterator<MythUIType *> i(m_childrenList);
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
        img->SetArea(MythRect(img->GetArea().translated(left, top)));
    }
}

void TeletextScreen::Pulse()
{
    if (!InitialiseFont() || !m_displaying)
        return;

    if (m_player && m_player->GetVideoOutput())
    {
        static const float kTextPadding = 0.96F;
        QRect oldsafe = m_safeArea;
        m_safeArea = m_player->GetVideoOutput()->GetSafeRect();
        m_colWidth = (int)((float)m_safeArea.width() / (float)kTeletextColumns);
        m_rowHeight = (int)((float)m_safeArea.height() / (float)kTeletextRows);

        if (oldsafe != m_safeArea)
        {
            m_teletextReader->SetPageChanged(true);

            int max_width  = (int)((float)m_colWidth * kTextPadding);
            m_fontHeight = (int)((float)m_rowHeight * kTextPadding);
            max_width = std::min(max_width, m_colWidth - 2);
            m_fontHeight = std::min(m_fontHeight, m_rowHeight - 2);
            gTTFont->GetFace()->setPixelSize(m_fontHeight);

            m_fontStretch = 200;
            bool ok = false;
            while (!ok && m_fontStretch > 50)
            {
                gTTFont->GetFace()->setStretch(m_fontStretch);
                QFontMetrics font(*(gTTFont->GetFace()));
                if (font.averageCharWidth() <= max_width || m_fontStretch < 50)
                    ok = true;
                else
                    m_fontStretch -= 10;
            }
        }
    }
    else
    {
        return;
    }

    if (!m_teletextReader->PageChanged())
        return;

    ClearScreen();

    const TeletextSubPage *ttpage = m_teletextReader->FindSubPage();

    if (!ttpage)
    {
        // no page selected so show the header and a list of available pages
        DrawHeader({}, 0);
        m_teletextReader->SetPageChanged(false);
        OptimiseDisplayedArea();
        return;
    }

    m_teletextReader->SetSubPage(ttpage->subpagenum);

    int a = 0;
    if ((ttpage->subtitle) ||
        (ttpage->flags & (TP_SUPPRESS_HEADER | TP_NEWSFLASH | TP_SUBTITLE)))
    {
        a = 1; // when showing subtitles we don't want to see the teletext
               // header line, so we skip that line...
        m_teletextReader->SetShowHeader(false);
        m_teletextReader->SetIsSubtitle(true);
    }
    else
    {
        m_teletextReader->SetShowHeader(true);
        m_teletextReader->SetIsSubtitle(false);
        DrawHeader(m_teletextReader->GetHeader(), ttpage->lang);
        m_teletextReader->SetHeaderChanged(false);
    }

    for (int y = kTeletextRows - a; y >= 2; y--)
        DrawLine(ttpage->data[y-1], y, ttpage->lang);

    m_teletextReader->SetPageChanged(false);
    OptimiseDisplayedArea();
}

bool TeletextScreen::KeyPress(const QString& Key, bool& Exit)
{
    if (m_teletextReader)
        return m_teletextReader->KeyPress(Key, Exit);
    return false;
}

void TeletextScreen::SetPage(int page, int subpage)
{
    if (m_teletextReader)
        m_teletextReader->SetPage(page, subpage);
}

void TeletextScreen::SetDisplaying(bool display)
{
    m_displaying = display;
    if (!m_displaying)
        ClearScreen();
}

void TeletextScreen::Reset()
{
    if (m_teletextReader)
        m_teletextReader->Reset();
}

void TeletextScreen::DrawHeader(const tt_line_array& page, int lang)
{
    if (!m_displaying)
        return;

    if (!page.empty())
        DrawLine(page, 1, lang);

    DrawStatus();
}

static QColor ttcolortoqcolor(int ttcolor)
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
    LOG(VB_VBI, LOG_DEBUG, QString("SetForegroundColor(%1)")
            .arg(TTColorToString(ttcolor)));

    gTTFont->SetColor(ttcolortoqcolor(ttcolor));
}

void TeletextScreen::SetBackgroundColor(int ttcolor)
{
    LOG(VB_VBI, LOG_DEBUG, QString("SetBackgroundColor(%1)")
            .arg(TTColorToString(ttcolor)));

    m_bgColor = ttcolortoqcolor(ttcolor);
    m_bgColor.setAlpha((ttcolor & kTTColorTransparent) ?
                       0x00 : gTTBackgroundAlpha);
}

void TeletextScreen::DrawLine(const tt_line_array& page, uint row, int lang)
{
    unsigned char last_ch = ' ';

    uint fgcolor    = kTTColorWhite;
    uint bgcolor    = kTTColorBlack;

    if (m_teletextReader->IsSubtitle() || m_teletextReader->IsTransparent())
    {
        bgcolor    = kTTColorTransparent;

        bool isBlank = true;
        for (uint i = (row == 1 ? 8 : 0); i < (uint) kTeletextColumns; i++)
        {
            unsigned char ch = page[i] & 0x7F;
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

    bool mosaic = false;
    [[maybe_unused]] bool seperation = false;
    bool conceal = false;
    [[maybe_unused]] bool flash = false;
    bool doubleheight = false;
    [[maybe_unused]] bool blink = false;
    bool hold = false;
    bool endbox = false;
    bool startbox = false;
    bool withinbox = false;
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
            withinbox = true;
        }

        if (endbox)
        {
            bgcolor = old_bgcolor;
            endbox = false;
            withinbox = false;
        }

        SetForegroundColor(fgcolor);
        SetBackgroundColor(bgcolor);

        unsigned char ch = page[x] & 0x7F;
        bool reserved = false;
        switch (ch)
        {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x04: case 0x05: case 0x06: case 0x07: // alpha + foreground color
                fgcolor = ch & 7;
                mosaic = false;
                conceal = false;
                // increment FLOF/FastText count if menu item detected
                flof_link_count += (row == 25) ? 1 : 0;
                break;
            case 0x08: // flash
                // XXX
                break;
            case 0x09: // steady
                flash = false;
                break;
            case 0x0a: // end box
                endbox = true;
                break;
            case 0x0b: // start box
                if (x < kTeletextColumns - 1 && ((page[x + 1] & 0x7F) == 0x0b))
                    startbox = true;
                break;
            case 0x0c: // normal height
                doubleheight = false;
                break;
            case 0x0d: // double height
                doubleheight = (row < (kTeletextRows-1)) && (x < (kTeletextColumns - 1));
                break;

            case 0x10: case 0x11: case 0x12: case 0x13:
            case 0x14: case 0x15: case 0x16: case 0x17: // graphics + foreground color
                fgcolor = ch & 7;
                mosaic = true;
                conceal = false;
                break;
            case 0x18: // conceal display
                conceal = true;
                break;
            case 0x19: // contiguous graphics
                seperation = false;
                break;
            case 0x1a: // separate graphics
                seperation = true;
                break;
            case 0x1c: // black background
                bgcolor = kTTColorBlack;
                break;
            case 0x1d: // new background
                bgcolor = fgcolor;
                break;
            case 0x1e: // hold graphics
                hold = true;
                break;
            case 0x1f: // release graphics
                hold = false;
                break;
            case 0x0e: // SO (reserved, double width)
            case 0x0f: // SI (reserved, double size)
            case 0x1b: // ESC (reserved)
                reserved = true;
                break;

            default:
                if ((ch >= 0x80) && (ch <=0x9f)) // these aren't used
                    ch = ' '; // BAD_CHAR;
                else
                {
                    if (conceal && !m_teletextReader->RevealHidden())
                        ch = ' ';
                }
                break;
        }

        // Extra processing for control characters
        if (ch < 0x20)
            ch = (hold && mosaic && !reserved) ? last_ch : ' ';

        // Hide FastText/FLOF menu characters if not available
        if (flof_link_count && (flof_link_count <= 6))
        {
            const TeletextSubPage *ttpage = m_teletextReader->FindSubPage();

            if (ttpage)
            {
                bool has_flof = ttpage->floflink[flof_link_count - 1] != 0;
                ch = (has_flof) ? ch : ' ';
            }
        }

        uint newfgcolor = fgcolor;
        uint newbgcolor = bgcolor;

        SetForegroundColor(newfgcolor);
        SetBackgroundColor(newbgcolor);
        if ((row != 0) || (x > 7))
        {
            if (m_teletextReader->IsTransparent())
                SetBackgroundColor(kTTColorTransparent);

            if (withinbox || !m_teletextReader->IsSubtitle())
            {
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
                    QChar c2 = cvt_char(ch, lang);
                    DrawCharacter(x, row, c2, doubleheight);
                }
            }
        }
    }
}

void TeletextScreen::DrawCharacter(int x, int y, QChar ch, bool doubleheight)
{
    QString line = ch;
    if (line == " ")
        return;

    int row = y;
    x *= m_colWidth;
    y *= m_rowHeight;
    int height = m_rowHeight * (doubleheight ? 2 : 1);
    QRect rect(x, y, m_colWidth, height);

    if (doubleheight)
    {
        gTTFont->GetFace()->setPixelSize(m_fontHeight * 2);
        gTTFont->GetFace()->setStretch(m_fontStretch / 2);
    }

    QImage* image = GetRowImage(row, rect);
    if (image)
    {
        QPainter painter(image);
        painter.setFont(gTTFont->face());
        painter.setPen(gTTFont->color());
        painter.drawText(rect, Qt::AlignCenter, line);
        painter.end();
    }

    if (row & 1)
    {
        row++;
        rect = QRect(x, y + m_rowHeight, m_colWidth, height);
        rect.translate(0, -m_rowHeight);
        image = GetRowImage(row, rect);
        if (image)
        {
            QPainter painter(image);
            painter.setFont(gTTFont->face());
            painter.setPen(gTTFont->color());
            painter.drawText(rect, Qt::AlignCenter, line);
            painter.end();
        }
    }

    if (doubleheight)
    {
        gTTFont->GetFace()->setPixelSize(m_fontHeight);
        gTTFont->GetFace()->setStretch(m_fontStretch);
    }
}

void TeletextScreen::DrawBackground(int x, int y)
{
    int row = y;
    x *= m_colWidth;
    y *= m_rowHeight;
    DrawRect(row, QRect(x, y, m_colWidth, m_rowHeight));
}

void TeletextScreen::DrawRect(int row, QRect rect)
{
    QImage* image = GetRowImage(row, rect);
    if (!image)
        return;

    QBrush bgfill = QBrush(m_bgColor, Qt::SolidPattern);
    QPainter painter(image);
    painter.setBrush(bgfill);
    painter.setPen(QPen(Qt::NoPen));
    painter.drawRect(rect);
    painter.end();
}

void TeletextScreen::DrawMosaic(int x, int y, int code, bool doubleheight)
{
    int row = y;
    x *= m_colWidth;
    y *= m_rowHeight;

    int dx = (int)round((double)m_colWidth / 2) + 1;
    int dy = (int)round((double)m_rowHeight / 3) + 1;
    dy = (doubleheight) ? (2 * dy) : dy;

    if (code & 0x10)
        DrawRect(row, QRect(x,      y + (2*dy), dx, dy));
    if (code & 0x40)
        DrawRect(row, QRect(x + dx, y + (2*dy), dx, dy));
    if (code & 0x01)
        DrawRect(row, QRect(x,      y,        dx, dy));
    if (code & 0x02)
        DrawRect(row, QRect(x + dx, y,        dx, dy));
    if (code & 0x04)
        DrawRect(row, QRect(x,      y + dy,   dx, dy));
    if (code & 0x08)
        DrawRect(row, QRect(x + dx, y + dy,   dx, dy));
}

void TeletextScreen::DrawStatus()
{
    SetForegroundColor(kTTColorWhite);
    SetBackgroundColor(kTTColorBlack);

    if (!m_teletextReader->IsTransparent())
        for (int i = 0; i < 40; ++i)
            DrawBackground(i, 0);

    DrawCharacter(1, 0, 'P', false);
    DrawCharacter(2, 0, QChar(m_teletextReader->GetPageInput(0)), false);
    DrawCharacter(3, 0, QChar(m_teletextReader->GetPageInput(1)), false);
    DrawCharacter(4, 0, QChar(m_teletextReader->GetPageInput(2)), false);

    const TeletextSubPage *ttpage = m_teletextReader->FindSubPage();

    if (!ttpage)
    {
        QString str = QObject::tr("Page Not Available",
                                  "Requested Teletext page not available");
        for (int i = 0; (i < 30) && i < str.length(); i++)
            DrawCharacter(i+10, 0, str[i], false);

        return;
    }

    QString str = m_teletextReader->GetPage();
    if (str.isEmpty())
        return;

    SetForegroundColor(kTTColorWhite);
    for (int x = 0; x < 11; x++)
    {
        if (m_teletextReader->IsTransparent())
            SetBackgroundColor(kTTColorTransparent);
        else
            SetBackgroundColor(kTTColorBlack);

        DrawBackground((x * 3) + 7, 0);

        if (str[x * 3] == '*')
        {
            str[x * 3] = ' ';
            SetBackgroundColor(kTTColorRed);
        }

        DrawBackground((x * 3) + 8, 0);
        DrawBackground((x * 3) + 9, 0);

        DrawCharacter((x * 3) + 7, 0, str[x * 3], false);
        DrawCharacter((x * 3) + 8, 0, str[(x * 3) + 1], false);
        DrawCharacter((x * 3) + 9, 0, str[(x * 3) + 2], false);
    }
}

bool TeletextScreen::InitialiseFont()
{
    static bool s_initialised = false;
    //QString font = gCoreContext->GetSetting("DefaultSubtitleFont", "FreeMono");
    if (s_initialised)
    {
        return true;
#if 0
        if (gTTFont->face().family() == font)
            return true;
        delete gTTFont;
#endif // 0
    }

    auto *mythfont = new MythFontProperties();
    QString font = SubtitleScreen::GetTeletextFontName();
    if (mythfont)
    {
        QFont newfont(font);
        mythfont->SetFace(newfont);
        gTTFont = mythfont;
    }
    else
    {
        return false;
    }

    gTTBackgroundAlpha = SubtitleScreen::GetTeletextBackgroundAlpha();

    s_initialised = true;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Loaded main subtitle font '%1'")
        .arg(font));
    return true;
}
