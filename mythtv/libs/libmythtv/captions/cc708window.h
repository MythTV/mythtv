// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef CC708_WINDOW_H
#define CC708_WINDOW_H

#include <array>
#include <utility>
#include <vector>

// Qt headers
#include <QString>
#include <QRecursiveMutex>
#include <QColor>

// MythTV headers
#include "libmythtv/mythtvexp.h"

extern const uint k708JustifyLeft;
extern const uint k708JustifyRight;
extern const uint k708JustifyCenter;
extern const uint k708JustifyFull;

extern const uint k708EffectSnap;
extern const uint k708EffectFade;
extern const uint k708EffectWipe;

extern const uint k708BorderNone;
extern const uint k708BorderRaised;
extern const uint k708BorderDepressed;
extern const uint k708BorderUniform;
extern const uint k708BorderShadowLeft;
extern const uint k708BorderShadowRight;

extern const uint k708DirLeftToRight;
extern const uint k708DirRightToLeft;
extern const uint k708DirTopToBottom;
extern const uint k708DirBottomToTop;

extern const uint k708AttrSizeSmall;
extern const uint k708AttrSizeStandard;
extern const uint k708AttrSizeLarge;

extern const uint k708AttrOffsetSubscript;
extern const uint k708AttrOffsetNormal;
extern const uint k708AttrOffsetSuperscript;

extern const uint k708AttrFontDefault;
extern const uint k708AttrFontMonospacedSerif;
extern const uint k708AttrFontProportionalSerif;
extern const uint k708AttrFontMonospacedSansSerif;
extern const uint k708AttrFontProportionalSansSerif;
extern const uint k708AttrFontCasual;
extern const uint k708AttrFontCursive;
extern const uint k708AttrFontSmallCaps;

extern const uint k708AttrEdgeNone;
extern const uint k708AttrEdgeRaised;
extern const uint k708AttrEdgeDepressed;
extern const uint k708AttrEdgeUniform;
extern const uint k708AttrEdgeLeftDropShadow;
extern const uint k708AttrEdgeRightDropShadow;

extern const uint k708AttrColorBlack;
extern const uint k708AttrColorWhite;

extern const uint k708AttrOpacitySolid;
extern const uint k708AttrOpacityFlash;
extern const uint k708AttrOpacityTranslucent;
extern const uint k708AttrOpacityTransparent;

const int k708MaxWindows = 8;
const int k708MaxRows    = 16; // 4-bit field in DefineWindow
const int k708MaxColumns = 64; // 6-bit field in DefineWindow

// Weird clang-tidy error from cc708window.cpp:212 that the attribute
// assignment is using zero allocated memory. Changing that code to
// explicitly copy each field of the structure, and then changing the
// first copy statement to assign a constant instead, shows that the
// warning is about the left side of the assignment statement. That
// should overwrite any zero-allocated memory, not reference it.
// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
class CC708CharacterAttribute
{
  public:
    uint   m_penSize         {k708AttrSizeStandard};
    uint   m_offset          {k708AttrOffsetNormal};
    uint   m_textTag         {0};
    uint   m_fontTag         {0};                    // system font
    uint   m_edgeType        {k708AttrEdgeNone};
    bool   m_underline       {false};
    bool   m_italics         {false};
    bool   m_boldface        {false};

    uint   m_fgColor         {k708AttrColorWhite};   // will be overridden
    uint   m_fgOpacity       {k708AttrOpacitySolid}; // solid
    uint   m_bgColor         {k708AttrColorBlack};
    uint   m_bgOpacity       {k708AttrOpacitySolid};
    uint   m_edgeColor       {k708AttrColorBlack};

    QColor m_actualFgColor;   // if !isValid(), then convert m_fgColor

    explicit CC708CharacterAttribute(bool isItalic = false,
                            bool isBold = false,
                            bool isUnderline = false,
                            QColor fgColor = QColor()) :
        m_underline(isUnderline),
        m_italics(isItalic),
        m_boldface(isBold),
        // NOLINTNEXTLINE(performance-move-const-arg)
        m_actualFgColor(std::move(fgColor))
    {
    }

    static QColor ConvertToQColor(uint eia708color);
    QColor GetFGColor(void) const
    {
        QColor fg = (m_actualFgColor.isValid() ?
                     m_actualFgColor : ConvertToQColor(m_fgColor));
        fg.setAlpha(GetFGAlpha());
        return fg;
    }
    QColor GetBGColor(void) const
    {
        QColor bg = ConvertToQColor(m_bgColor);
        bg.setAlpha(GetBGAlpha());
        return bg;
    }
    QColor GetEdgeColor(void) const { return ConvertToQColor(m_edgeColor); }

    uint GetFGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static constexpr std::array<const uint,4> alpha = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_fgOpacity & 0x3];
    }

    uint GetBGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static constexpr std::array<const uint,4> alpha = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_bgOpacity & 0x3];
    }

    bool operator==(const CC708CharacterAttribute &other) const;
    bool operator!=(const CC708CharacterAttribute &other) const
        { return !(*this == other); }
};

class CC708Pen
{
  public:
    CC708Pen() = default;
    void SetPenStyle(uint style);
    void SetAttributes(int pen_size,
                       int offset,       int text_tag,  int font_tag,
                       int edge_type,    int underline, int italics)
    {
        m_attr.m_penSize   = pen_size;
        m_attr.m_offset    = offset;
        m_attr.m_textTag   = text_tag;
        m_attr.m_fontTag   = font_tag;
        m_attr.m_edgeType  = edge_type;
        m_attr.m_underline = (underline != 0);
        m_attr.m_italics   = (italics != 0);
        m_attr.m_boldface  = false;
    }
  public:
    CC708CharacterAttribute m_attr;

    uint m_row    {0};
    uint m_column {0};
};

class CC708Window;
class CC708Character
{
  public:
    CC708Character() = default;
    explicit CC708Character(const CC708Window &win);
    CC708CharacterAttribute m_attr;
    QChar                   m_character {' '};
};

class CC708String
{
  public:
    uint                    m_x {0};
    uint                    m_y {0};
    QString                 m_str;
    CC708CharacterAttribute m_attr;
};

class MTV_PUBLIC CC708Window
{
 public:
    CC708Window() = default;
    ~CC708Window();

    void DefineWindow(int priority,         bool visible,
                      int anchor_point,     int relative_pos,
                      int anchor_vertical,  int anchor_horizontal,
                      int row_count,        int column_count,
                      int row_lock,         int column_lock,
                      int pen_style,        int window_style);
    void Resize(uint new_rows, uint new_columns);
    void Clear(void);
    void SetWindowStyle(uint style);

    void AddChar(QChar ch);
    void IncrPenLocation(void);
    void DecrPenLocation(void);
    void SetPenLocation(uint row, uint column);
    void LimitPenLocation(void);

    bool IsPenValid(void) const
    {
        return ((m_pen.m_row < m_true_row_count) &&
                (m_pen.m_column < m_true_column_count));
    }
    CC708Character &GetCCChar(void) const;
    std::vector<CC708String*> GetStrings(void) const;
    static void DisposeStrings(std::vector<CC708String*> &strings);
    QColor GetFillColor(void) const
    {
        QColor fill = CC708CharacterAttribute::ConvertToQColor(m_fill_color);
        fill.setAlpha(GetFillAlpha());
        return fill;
    }
    uint GetFillAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static constexpr std::array<const uint,4> alpha = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[m_fill_opacity & 0x3];
    }

 private:
    void Scroll(int row, int col);

 public:
    uint            m_priority          {0};
 private:
    bool            m_visible           {false};
 public:
    enum : std::uint8_t {
        kAnchorUpperLeft  = 0, kAnchorUpperCenter = 1, kAnchorUpperRight  = 2,
        kAnchorCenterLeft = 3, kAnchorCenter      = 4, kAnchorCenterRight = 5,
        kAnchorLowerLeft  = 6, kAnchorLowerCenter = 7, kAnchorLowerRight  = 8,
    };
    uint            m_anchor_point      {0};
    uint            m_relative_pos      {0};
    uint            m_anchor_vertical   {0};
    uint            m_anchor_horizontal {0};
    uint            m_row_count         {0};
    uint            m_column_count      {0};
    uint            m_row_lock          {0};
    uint            m_column_lock       {0};
//  uint            m_pen_style         {0};
//  uint            m_window_style      {0};

    uint            m_fill_color        {0};
    uint            m_fill_opacity      {0};
    uint            m_border_color      {0};
    uint            m_border_type       {0};
    uint            m_scroll_dir        {0};
    uint            m_print_dir         {0};
    uint            m_effect_dir        {0};
    uint            m_display_effect    {0};
    uint            m_effect_speed      {0};
    uint            m_justify           {0};
    uint            m_word_wrap         {0};

    // These are akin to the capacity of a vector, which is always >=
    // the current size.
    uint            m_true_row_count    {0};
    uint            m_true_column_count {0};

    CC708Character *m_text              {nullptr};
    CC708Pen        m_pen;

 private:
    /// set to false when DeleteWindow is called on the window.
    bool            m_exists            {false};
    bool            m_changed           {true};

 public:
    bool GetExists(void) const { return m_exists; }
    bool GetVisible(void) const { return m_visible; }
    bool GetChanged(void) const { return m_changed; }
    void SetExists(bool value)
    {
        if (m_exists != value)
            SetChanged();
        m_exists = value;
    }
    void SetVisible(bool value)
    {
        if (m_visible != value)
            SetChanged();
        m_visible = value;
    }
    void SetChanged(void)
    {
        m_changed = true;
    }
    void ResetChanged(void)
    {
        m_changed = false;
    }
    mutable QRecursiveMutex  m_lock;
};

class CC708Service
{
  public:
    CC708Service() = default;

  public:
    uint        m_currentWindow {0};
    std::array<CC708Window,k708MaxWindows> m_windows;
};

#endif // CC708_WINDOW_H
