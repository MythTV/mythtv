// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#include <cassert>
#include <algorithm>

#include "libmythbase/mythlogging.h"
#include "captions/cc708window.h"

/************************************************************************

    FCC Addons to EIA-708.

    * Decoders must support the standard, large, and small caption sizes
      and must allow the caption provider to choose a size and allow the
      viewer to choose an alternative size.

    * Decoders must support the eight fonts listed in EIA-708. Caption
      providers may specify 1 of these 8 font styles to be used to write
      caption text. Decoders must include the ability for consumers to
      choose among the eight fonts. The decoder must display the font
      chosen by the caption provider unless the viewer chooses a different
      font.

    * Decoders must implement the same 8 character background colors
      as those that Section 9 requires be implemented for character
      foreground (white, black, red, green, blue, yellow, magenta and cyan).

    * Decoders must implement options for altering the appearance of
      caption character edges.

    * Decoders must display the color chosen by the caption provider,
      and must allow viewers to override the foreground and/or background
      color chosen by the caption provider and select alternate colors.

    * Decoders must be capable of decoding and processing data for the
      six standard services, but information from only one service need
      be displayed at a given time.

    * Decoders must include an option that permits a viewer to choose a
      setting that will display captions as intended by the caption
      provider (a default). Decoders must also include an option that
      allows a viewer's chosen settings to remain until the viewer
      chooses to alter these settings, including during periods when
      the television is turned off.

    * Cable providers and other multichannel video programming
      distributors must transmit captions in a format that will be
      understandable to this decoder circuitry in digital cable
      television sets when transmitting programming to digital
      television devices.

******************************************************************************/

const uint k708JustifyLeft            = 0;
const uint k708JustifyRight           = 1;
const uint k708JustifyCenter          = 2;
const uint k708JustifyFull            = 3;

const uint k708EffectSnap             = 0;
const uint k708EffectFade             = 1;
const uint k708EffectWipe             = 2;

const uint k708BorderNone             = 0;
const uint k708BorderRaised           = 1;
const uint k708BorderDepressed        = 2;
const uint k708BorderUniform          = 3;
const uint k708BorderShadowLeft       = 4;
const uint k708BorderShadowRight      = 5;

const uint k708DirLeftToRight         = 0;
const uint k708DirRightToLeft         = 1;
const uint k708DirTopToBottom         = 2;
const uint k708DirBottomToTop         = 3;

const uint k708AttrSizeSmall          = 0;
const uint k708AttrSizeStandard       = 1;
const uint k708AttrSizeLarge          = 2;

const uint k708AttrOffsetSubscript    = 0;
const uint k708AttrOffsetNormal       = 1;
const uint k708AttrOffsetSuperscript  = 2;

const uint k708AttrFontDefault               = 1;
const uint k708AttrFontMonospacedSerif       = 1;
const uint k708AttrFontProportionalSerif     = 2;
const uint k708AttrFontMonospacedSansSerif   = 3;
const uint k708AttrFontProportionalSansSerif = 4;
const uint k708AttrFontCasual                = 5;
const uint k708AttrFontCursive               = 6;
const uint k708AttrFontSmallCaps             = 7;

const uint k708AttrEdgeNone            = 0;
const uint k708AttrEdgeRaised          = 1;
const uint k708AttrEdgeDepressed       = 2;
const uint k708AttrEdgeUniform         = 3;
const uint k708AttrEdgeLeftDropShadow  = 4;
const uint k708AttrEdgeRightDropShadow = 5;

const uint k708AttrColorBlack         = 0;
const uint k708AttrColorWhite         = 63;

const uint k708AttrOpacitySolid       = 0;
const uint k708AttrOpacityFlash       = 1;
const uint k708AttrOpacityTranslucent = 2;
const uint k708AttrOpacityTransparent = 3;

void CC708Window::DefineWindow(int _priority,         bool _visible,
                               int _anchor_point,     int _relative_pos,
                               int _anchor_vertical,  int _anchor_horizontal,
                               int _row_count,        int _column_count,
                               int _row_lock,         int _column_lock,
                               int _pen_style,        int _window_style)
{
    // The DefineWindow command may be sent frequently to allow a
    // caption decoder just tuning in to get in synch quickly.
    // Usually the row_count and column_count are unchanged, but it is
    // possible to add or remove rows or columns.  Due to the
    // one-dimensional row-major representation of characters, if the
    // number of columns is changed, a new array must be created and
    // the old characters copied in.  If only the number of rows
    // decreases, the array can be left unchanged.  If only the number
    // of rows increases, the old characters can be copied into the
    // new character array directly without any index translation.
    QMutexLocker locker(&m_lock);

    _row_count++;
    _column_count++;

    m_priority          = _priority;
    SetVisible(_visible);
    m_anchor_point      = _anchor_point;
    m_relative_pos      = _relative_pos;
    m_anchor_vertical   = _anchor_vertical;
    m_anchor_horizontal = _anchor_horizontal;
    m_row_lock          = _row_lock;
    m_column_lock       = _column_lock;

    if ((!_pen_style && !GetExists()) || _pen_style)
        m_pen.SetPenStyle(_pen_style ? _pen_style : 1);

    if ((!_window_style && !GetExists()) || _window_style)
        SetWindowStyle(_window_style ? _window_style : 1);

    Resize(_row_count, _column_count);
    m_row_count = _row_count;
    m_column_count = _column_count;
    LimitPenLocation();

    SetExists(true);
}

// Expand the internal array of characters if necessary to accommodate
// the current values of row_count and column_count.  Any new (space)
// characters exposed are given the current pen attributes.  At the
// end, row_count and column_count are NOT updated.
void CC708Window::Resize(uint new_rows, uint new_columns)
{
    // Validate arguments.
    if (new_rows == 0 || new_columns == 0)
    {
      LOG(VB_VBI,
          LOG_DEBUG,
          QString("Invalid arguments to Resize: %1 rows x %2 columns")
          .arg(new_rows).arg(new_columns));
      return;
    }

    if (!GetExists() || m_text == nullptr)
    {
        m_true_row_count = 0;
        m_true_column_count = 0;
    }

    //We need to shrink Rows, at times Scroll (row >= (int)m_true_row_count)) fails and we
    // Don't scroll caption line resulting in overwriting the same.
    // Ex: [CAPTIONING FUNDED BY CBS SPORTS
    //     DIVISION]NG FUNDED BY CBS SPORTS

    if (new_rows < m_true_row_count || new_columns < m_true_column_count)
    {
      delete [] m_text;
      m_text = new CC708Character [static_cast<size_t>(new_rows) * new_columns];
      m_true_row_count = new_rows;
      m_true_column_count = new_columns;
      m_pen.m_row = 0;
      m_pen.m_column = 0;
      Clear();
      SetChanged();
      SetExists(true);
      LOG(VB_VBI,
          LOG_DEBUG,
          QString("Shrinked nr %1 nc %2 rc %3 cc %4 tr %5 tc %6").arg(new_rows)
          .arg(new_columns) .arg(m_row_count) .arg(m_column_count)
          .arg(m_true_row_count) .arg(m_true_column_count));
      return;
    }

    if (new_rows > m_true_row_count || new_columns > m_true_column_count)
    {
        new_rows = std::max(new_rows, m_true_row_count);
        new_columns = std::max(new_columns, m_true_column_count);

        // Expand the array if the new size exceeds the current capacity
        // in either dimension.
        auto *new_text = new CC708Character[static_cast<size_t>(new_rows) * new_columns];
        m_pen.m_column = 0;
        m_pen.m_row = 0;
        uint i = 0;
        for (i = 0; m_text && i < m_row_count; ++i)
        {
            uint j = 0;
            for (j = 0; j < m_column_count; ++j)
                new_text[(i * new_columns) + j] = m_text[(i * m_true_column_count) + j];
            for (; j < new_columns; ++j)
                new_text[(i * new_columns) + j].m_attr = m_pen.m_attr;
        }
        for (; i < new_rows; ++i)
            for (uint j = 0; j < new_columns; ++j)
                new_text[(i * new_columns) + j].m_attr = m_pen.m_attr;

        delete [] m_text;
        m_text = new_text;
        m_true_row_count = new_rows;
        m_true_column_count = new_columns;
        SetChanged();
    }
    else if (new_rows > m_row_count || new_columns > m_column_count)
    {
        // At least one dimension expanded into existing space, so
        // those newly exposed characters must be cleared.
        for (uint i = 0; i < m_row_count; ++i)
        {
            for (uint j = m_column_count; j < new_columns; ++j)
            {
                m_text[(i * m_true_column_count) + j].m_character = ' ';
                m_text[(i * m_true_column_count) + j].m_attr = m_pen.m_attr;
            }
        }
        for (uint i = m_row_count; i < new_rows; ++i)
        {
            for (uint j = 0; j < new_columns; ++j)
            {
                m_text[(i * m_true_column_count) + j].m_character = ' ';
                m_text[(i * m_true_column_count) + j].m_attr = m_pen.m_attr;
            }
        }
        SetChanged();
    }
    SetExists(true);
}


CC708Window::~CC708Window()
{
    QMutexLocker locker(&m_lock);

    SetExists(false);
    m_true_row_count    = 0;
    m_true_column_count = 0;

    if (m_text)
    {
        delete [] m_text;
        m_text = nullptr;
    }
}

void CC708Window::Clear(void)
{
    QMutexLocker locker(&m_lock);

    if (!GetExists() || !m_text)
        return;

    for (uint i = 0; i < m_true_row_count * m_true_column_count; i++)
    {
        m_text[i].m_character = QChar(' ');
        m_text[i].m_attr = m_pen.m_attr;
    }
    SetChanged();
}

CC708Character &CC708Window::GetCCChar(void) const
{
    assert(GetExists());
    assert(m_text);
    assert(m_pen.m_row    < m_true_row_count);
    assert(m_pen.m_column < m_true_column_count);
    return m_text[(m_pen.m_row * m_true_column_count) + m_pen.m_column];
}

std::vector<CC708String*> CC708Window::GetStrings(void) const
{
    // Note on implementation.  In many cases, one line will be
    // computed as the concatenation of 3 strings: a prefix of spaces
    // with a default foreground color, followed by the actual text in
    // a specific foreground color, followed by a suffix of spaces
    // with a default foreground color.  This leads to 3
    // FormattedTextChunk objects when 1 would suffice.  The prefix
    // and suffix ultimately get optimized away, but not before a
    // certain amount of unnecessary work.
    //
    // This can be solved with two steps.  First, suppress a format
    // change when only non-underlined spaces have been seen so far.
    // (Background changes can be ignored because the subtitle code
    // suppresses leading spaces.)  Second, for trailing
    // non-underlined spaces, either suppress a format change, or
    // avoid creating such a string when it appears at the end of the
    // row.  (We can't do the latter for an initial string of spaces,
    // because the spaces are needed for coordinate calculations.)
    QMutexLocker locker(&m_lock);

    std::vector<CC708String*> list;

    CC708String *cur = nullptr;

    if (!m_text)
        return list;

    bool createdNonblankStrings = false;
    std::array<QChar,k708MaxColumns> chars {};
    for (uint j = 0; j < m_row_count; j++)
    {
        bool inLeadingSpaces = true;
        bool inTrailingSpaces = true;
        bool createdString = false;
        uint strStart = 0;
        for (uint i = 0; i < m_column_count; i++)
        {
            CC708Character &chr = m_text[(j * m_true_column_count) + i];
            chars[i] = chr.m_character;
            if (!cur)
            {
                cur = new CC708String;
                cur->m_x    = i;
                cur->m_y    = j;
                cur->m_attr = chr.m_attr;
                strStart = i;
            }
            bool isDisplayable = (chr.m_character != ' ' || chr.m_attr.m_underline);
            if (inLeadingSpaces && isDisplayable)
            {
                cur->m_attr = chr.m_attr;
                inLeadingSpaces = false;
            }
            if (isDisplayable)
            {
                inTrailingSpaces = false;
            }
            if (cur->m_attr != chr.m_attr)
            {
                cur->m_str = QString(&chars[strStart], i - strStart);
                list.push_back(cur);
                createdString = true;
                createdNonblankStrings = true;
                inTrailingSpaces = true;
                cur = nullptr;
                i--;
            }
        }
        if (cur)
        {
            // If the entire string is spaces, we still may need to
            // create a chunk to preserve spacing between lines.
            if (!inTrailingSpaces || !createdString)
            {
                bool allSpaces = (inLeadingSpaces || inTrailingSpaces);
                int length = allSpaces ? 0 : m_column_count - strStart;
                if (length)
                    createdNonblankStrings = true;
                cur->m_str = QString(&chars[strStart], length);
                list.push_back(cur);
            }
            else
            {
                delete cur;
            }
            cur = nullptr;
        }
    }
    if (!createdNonblankStrings)
        list.clear();
    return list;
}

void CC708Window::DisposeStrings(std::vector<CC708String*> &strings)
{
    while (!strings.empty())
    {
        delete strings.back();
        strings.pop_back();
    }
}

void CC708Window::SetWindowStyle(uint style)
{
    static const std::array<const uint,8> style2justify
    {
        k708JustifyLeft, k708JustifyLeft, k708JustifyLeft,   k708JustifyCenter,
        k708JustifyLeft, k708JustifyLeft, k708JustifyCenter, k708JustifyLeft,
    };

    if ((style < 1) || (style > 7))
        return;

    m_fill_color     = k708AttrColorBlack;
    m_fill_opacity   = ((2 == style) || (5 == style)) ?
        k708AttrOpacityTransparent : k708AttrOpacitySolid;
    m_border_color   = k708AttrColorBlack;
    m_border_type    = k708BorderNone;
    m_scroll_dir     = (style < 7) ? k708DirBottomToTop : k708DirRightToLeft;
    m_print_dir      = (style < 7) ? k708DirLeftToRight : k708DirTopToBottom;
    m_effect_dir     = m_scroll_dir;
    m_display_effect = k708EffectSnap;
    m_effect_speed   = 0;
    m_justify        = style2justify[style];
    m_word_wrap      = (style > 3) && (style < 7) ? 1 : 0;

    /// HACK -- begin
    // It appears that ths is missused by broadcasters (FOX -- Dollhouse)
    m_fill_opacity   = k708AttrOpacityTransparent;
    /// HACK -- end
}

void CC708Window::AddChar(QChar ch)
{
    if (!GetExists())
        return;

    QString dbg_char = ch;
    if (ch.toLatin1() < 32)
        dbg_char = QString("0x%1").arg( (int)ch.toLatin1(), 0,16);

    if (!IsPenValid())
    {
        LOG(VB_VBI, LOG_DEBUG,
            QString("AddChar(%1) at (c %2, r %3) INVALID win(%4,%5)")
                .arg(dbg_char).arg(m_pen.m_column).arg(m_pen.m_row)
                .arg(m_true_column_count).arg(m_true_row_count));
        return;
    }

    // CEA-708-D Section 7.1.4, page 30
    // Carriage Return (CR) moves the current entry point to the beginning of the next row. If the next row is
    // below the visible window, the window "rolls up" as defined in CEA-608-E Section 7.4. If the next row is
    // within the visible window and contains text, the cursor is moved to the beginning of the row, but the pre-
    // existing text is not erased.
    if (ch.toLatin1() == 0x0D)      // C0::CR
    {
        Scroll(m_pen.m_row + 1, 0);
        SetChanged();
        return;
    }

    QMutexLocker locker(&m_lock);

    // CEA-708-D Section 7.1.4, page 30
    // Horizontal Carriage Return (HCR) moves the current entry point to the beginning of the current row
    // without row increment or decrement. It shall erase all text on the row.
    if (ch.toLatin1() == 0x0E)      // C0::HCR
    {
        uint p = m_pen.m_row * m_true_column_count;
        for (uint c = 0; c < m_column_count; c++)
        {
            m_text[c + p].m_attr      = m_pen.m_attr;
            m_text[c + p].m_character = QChar(' ');
        }
        m_pen.m_column = 0;
        LimitPenLocation();
        SetChanged();
        return;
    }

    // Backspace
    if (ch.toLatin1() == 0x08)
    {
        DecrPenLocation();
        CC708Character& chr = GetCCChar();
        chr.m_attr      = m_pen.m_attr;
        chr.m_character = QChar(' ');
        SetChanged();
        return;
    }

    // CEA-708-D Section 7.1.4, page 30
    // Form Feed (FF) erases all text in the window and moves the cursor to the first character position in the
    // window (0,0).
    if (ch.toLatin1() == 0x0c)      // C0::FF
    {
        Clear();
        SetPenLocation(0,0);
        SetChanged();
        return;
    }

    CC708Character& chr = GetCCChar();
    chr.m_attr      = m_pen.m_attr;
    chr.m_character = ch;
    int c = m_pen.m_column;
    int r = m_pen.m_row;
    IncrPenLocation();
    SetChanged();

    LOG(VB_VBI, LOG_DEBUG, QString("AddChar(%1) at (c %2, r %3) -> (%4,%5)")
            .arg(dbg_char).arg(c).arg(r).arg(m_pen.m_column).arg(m_pen.m_row));
}

void CC708Window::Scroll(int row, int col)
{
    QMutexLocker locker(&m_lock);

    if (!m_true_row_count || !m_true_column_count)
        return;

    if (m_text && (k708DirBottomToTop == m_scroll_dir) &&
        (row >= (int)m_true_row_count))
    {
        for (uint j = 0; j < m_true_row_count - 1; j++)
        {
            for (uint i = 0; i < m_true_column_count; i++)
                m_text[(m_true_column_count * j) + i] =
                    m_text[(m_true_column_count * (j+1)) + i];
        }
        //uint colsz = m_true_column_count * sizeof(CC708Character);
        //memmove(m_text, m_text + colsz, colsz * (m_true_row_count - 1));

        CC708Character tmp(*this);
        for (uint i = 0; i < m_true_column_count; i++)
            m_text[(m_true_column_count * (m_true_row_count - 1)) + i] = tmp;

        m_pen.m_row = m_true_row_count - 1;
        SetChanged();
    }
    else
    {
        m_pen.m_row = row;
    }
    // TODO implement other 3 scroll directions...

    m_pen.m_column = col;
}

void CC708Window::IncrPenLocation(void)
{
    // TODO: Scroll direction and up/down printing,
    // and word wrap not handled yet...
    int new_column = m_pen.m_column;
    int new_row = m_pen.m_row;

    new_column += (m_print_dir == k708DirLeftToRight) ? +1 : 0;
    new_column += (m_print_dir == k708DirRightToLeft) ? -1 : 0;
    new_row    += (m_print_dir == k708DirTopToBottom) ? +1 : 0;
    new_row    += (m_print_dir == k708DirBottomToTop) ? -1 : 0;

#if 0
    LOG(VB_VBI, LOG_DEBUG, QString("IncrPen dir%1: (c %2, r %3) -> (%4,%5)")
            .arg(m_print_dir).arg(m_pen.m_column).arg(m_pen.m_row)
            .arg(new_column).arg(new_row));
#endif

    if (k708DirLeftToRight == m_print_dir || k708DirRightToLeft == m_print_dir)
    {
        // basic wrapping for l->r, r->l languages
        if (!m_row_lock && m_column_lock && (new_column >= (int)m_true_column_count))
        {
            new_column  = 0;
            new_row    += 1;
        }
        else if (!m_row_lock && m_column_lock && (new_column < 0))
        {
            new_column  = (int)m_true_column_count - 1;
            new_row    -= 1;
        }
        Scroll(new_row, new_column);
    }
    else
    {
        m_pen.m_column = std::max(new_column, 0);
        m_pen.m_row    = std::max(new_row,    0);
    }
    // TODO implement other 2 scroll directions...

    LimitPenLocation();
}

void CC708Window::DecrPenLocation(void)
{
    // TODO: Scroll direction and up/down printing,
    // and word wrap not handled yet...
    int new_column = m_pen.m_column;
    int new_row = m_pen.m_row;

    new_column -= (m_print_dir == k708DirLeftToRight) ? +1 : 0;
    new_column -= (m_print_dir == k708DirRightToLeft) ? -1 : 0;
    new_row    -= (m_print_dir == k708DirTopToBottom) ? +1 : 0;
    new_row    -= (m_print_dir == k708DirBottomToTop) ? -1 : 0;

#if 0
    LOG(VB_VBI, LOG_DEBUG, QString("DecrPen dir%1: (c %2, r %3) -> (%4,%5)")
        .arg(m_print_dir).arg(m_pen.m_column).arg(m_pen.m_row)
            .arg(new_column).arg(new_row));
#endif

    if (k708DirLeftToRight == m_print_dir || k708DirRightToLeft == m_print_dir)
    {
        // basic wrapping for l->r, r->l languages
        if (!m_row_lock && m_column_lock && (new_column >= (int)m_true_column_count))
        {
            new_column  = 0;
            new_row    += 1;
        }
        else if (!m_row_lock && m_column_lock && (new_column < 0))
        {
            new_column  = (int)m_true_column_count - 1;
            new_row    -= 1;
        }
        Scroll(new_row, new_column);
    }
    else
    {
        m_pen.m_column = std::max(new_column, 0);
        m_pen.m_row    = std::max(new_row,    0);
    }
    // TODO implement other 2 scroll directions...

    LimitPenLocation();
}

void CC708Window::SetPenLocation(uint row, uint column)
{
  //Clear current row in case we are reseting Pen Location.
  LOG(VB_VBI,
      LOG_DEBUG,
      QString("SetPenLocation nr %1 nc %2 rc %3 cc %4 tr %5 tc %6").arg(row)
      .arg(column).arg(m_row_count).arg(m_column_count).arg(m_true_row_count)
      .arg(m_true_column_count));
     if(0 == row)
     {
          Scroll(m_true_row_count, column);
          m_pen.m_row = row;
     }
     else
     {
          Scroll(row, column);
     }
     LimitPenLocation();
}

void CC708Window::LimitPenLocation(void)
{
    // basic limiting
    uint max_col   = std::max((int)m_true_column_count - 1, 0);
    uint max_row   = std::max((int)m_true_row_count    - 1, 0);
    m_pen.m_column = std::min(m_pen.m_column, max_col);
    m_pen.m_row    = std::min(m_pen.m_row,    max_row);
}

/***************************************************************************/

void CC708Pen::SetPenStyle(uint style)
{
    static const std::array<const uint8_t,8> kStyle2Font
        { 0, 0, 1, 2, 3, 4, 3, 4 };

    if ((style < 1) || (style > 7))
        return;

    m_attr.m_penSize    = k708AttrSizeStandard;
    m_attr.m_offset     = k708AttrOffsetNormal;
    m_attr.m_fontTag    = kStyle2Font[style];
    m_attr.m_italics    = false;
    m_attr.m_underline  = false;
    m_attr.m_boldface   = false;
    m_attr.m_edgeType   = 0;
    m_attr.m_fgColor    = k708AttrColorWhite;
    m_attr.m_fgOpacity  = k708AttrOpacitySolid;
    m_attr.m_bgColor    = k708AttrColorBlack;
    m_attr.m_bgOpacity  = (style<6) ?
        k708AttrOpacitySolid : k708AttrOpacityTransparent;
    m_attr.m_edgeColor  = k708AttrColorBlack;
    m_attr.m_actualFgColor = QColor();
}

CC708Character::CC708Character(const CC708Window &win)
    : m_attr(win.m_pen.m_attr)
{
}

bool CC708CharacterAttribute::operator==(
    const CC708CharacterAttribute &other) const
{
    return ((m_penSize    == other.m_penSize)    &&
            (m_offset     == other.m_offset)     &&
            (m_textTag    == other.m_textTag)    &&
            (m_fontTag    == other.m_fontTag)    &&
            (m_edgeType   == other.m_edgeType)   &&
            (m_underline  == other.m_underline)  &&
            (m_italics    == other.m_italics)    &&
            (m_fgColor    == other.m_fgColor)    &&
            (m_fgOpacity  == other.m_fgOpacity)  &&
            (m_bgColor    == other.m_bgColor)    &&
            (m_bgOpacity  == other.m_bgOpacity)  &&
            (m_edgeColor  == other.m_edgeColor));
}

QColor CC708CharacterAttribute::ConvertToQColor(uint eia708color)
{
    // Color is expressed in 6 bits, 2 each for red, green, and blue.
    // U.S. ATSC programs seem to use just the higher-order bit,
    // i.e. values 0 and 2, so the last two elements of X[] are both
    // set to the maximum 255, otherwise font colors are dim.
    static constexpr std::array<const uint8_t,4> kX {0, 96, 255, 255};
    return {kX[(eia708color>>4)&3], kX[(eia708color>>2)&3], kX[eia708color&3]};
}
