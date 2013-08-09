// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef _CC708_WINDOW_
#define _CC708_WINDOW_

#include <vector>
using namespace std;

#include <QString>
#include <QMutex>
#include <QColor>

#include "mythtvexp.h"

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

class CC708CharacterAttribute
{
  public:
    uint pen_size;
    uint offset;
    uint text_tag;
    uint font_tag;
    uint edge_type;
    uint underline;
    uint italics;
    uint boldface;

    uint fg_color;
    uint fg_opacity;
    uint bg_color;
    uint bg_opacity;
    uint edge_color;

    QColor actual_fg_color; // if !isValid(), then convert fg_color

    CC708CharacterAttribute(bool isItalic = false,
                            bool isBold = false,
                            bool isUnderline = false,
                            QColor fgColor = QColor()) :
        pen_size(k708AttrSizeStandard),
        offset(k708AttrOffsetNormal),
        text_tag(0), // "dialog", ignored
        font_tag(0), // system font
        edge_type(k708AttrEdgeNone),
        underline(isUnderline),
        italics(isItalic),
        boldface(isBold),
        fg_color(k708AttrColorWhite), // will be overridden
        fg_opacity(k708AttrOpacitySolid), // solid
        bg_color(k708AttrColorBlack),
        bg_opacity(k708AttrOpacitySolid),
        edge_color(k708AttrColorBlack),
        actual_fg_color(fgColor)
    {
    }

    static QColor ConvertToQColor(uint eia708color);
    QColor GetFGColor(void) const
    {
        QColor fg = (actual_fg_color.isValid() ?
                     actual_fg_color : ConvertToQColor(fg_color));
        fg.setAlpha(GetFGAlpha());
        return fg;
    }
    QColor GetBGColor(void) const
    {
        QColor bg = ConvertToQColor(bg_color);
        bg.setAlpha(GetBGAlpha());
        return bg;
    }
    QColor GetEdgeColor(void) const { return ConvertToQColor(edge_color); }

    uint GetFGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[fg_opacity & 0x3];
    }

    uint GetBGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[bg_opacity & 0x3];
    }

    bool operator==(const CC708CharacterAttribute &other) const;
    bool operator!=(const CC708CharacterAttribute &other) const
        { return !(*this == other); }
};

class CC708Pen
{
  public:
    CC708Pen() : row(0), column(0) {}
    void SetPenStyle(uint style);
    void SetAttributes(int pen_size,
                       int offset,       int text_tag,  int font_tag,
                       int edge_type,    int underline, int italics)
    {
        attr.pen_size  = pen_size;
        attr.offset    = offset;
        attr.text_tag  = text_tag;
        attr.font_tag  = font_tag;
        attr.edge_type = edge_type;
        attr.underline = underline;
        attr.italics   = italics;
        attr.boldface  = 0;
    }
  public:
    CC708CharacterAttribute attr;

    uint row;
    uint column;
};

class CC708Window;
class CC708Character
{
  public:
    CC708Character() : character(' ') {}
    CC708Character(const CC708Window &win);
    CC708CharacterAttribute attr;
    QChar character;
};

class CC708String
{
  public:
    uint x;
    uint y;
    QString str;
    CC708CharacterAttribute attr;
};

class MTV_PUBLIC CC708Window
{
  public:
    CC708Window();
    ~CC708Window();

    void DefineWindow(int priority,         int visible,
                      int anchor_point,     int relative_pos,
                      int anchor_vertical,  int anchor_horizontal,
                      int row_count,        int column_count,
                      int row_lock,         int column_lock,
                      int pen_style,        int window_style);
    void Resize(uint new_rows, uint new_columns);
    void Clear(void);
    void SetWindowStyle(uint);

    void AddChar(QChar);
    void IncrPenLocation(void);
    void DecrPenLocation(void);
    void SetPenLocation(uint, uint);
    void LimitPenLocation(void);

    bool IsPenValid(void) const
    {
        return ((pen.row < true_row_count) &&
                (pen.column < true_column_count));
    }
    CC708Character &GetCCChar(void) const;
    vector<CC708String*> GetStrings(void) const;

    QColor GetFillColor(void) const
    {
        QColor fill = CC708CharacterAttribute::ConvertToQColor(fill_color);
        fill.setAlpha(GetFillAlpha());
        return fill;
    }
    uint GetFillAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0x7f, 0x00, };
        return alpha[fill_opacity & 0x3];
    }

  private:
    void Scroll(int row, int col);

  public:
    uint priority;
    uint visible;
    enum {
        kAnchorUpperLeft  = 0, kAnchorUpperCenter, kAnchorUpperRight,
        kAnchorCenterLeft = 3, kAnchorCenter,      kAnchorCenterRight,
        kAnchorLowerLeft  = 6, kAnchorLowerCenter, kAnchorLowerRight,
    };
    uint anchor_point;
    uint relative_pos;
    uint anchor_vertical;
    uint anchor_horizontal;
    uint row_count;
    uint column_count;
    uint row_lock;
    uint column_lock;
    uint pen_style;
    uint window_style;

    uint fill_color;
    uint fill_opacity;
    uint border_color;
    uint border_type;
    uint scroll_dir;
    uint print_dir;
    uint effect_dir;
    uint display_effect;
    uint effect_speed;
    uint justify;
    uint word_wrap;

    // These are akin to the capacity of a vector, which is always >=
    // the current size.
    uint true_row_count;
    uint true_column_count;

    CC708Character *text;
    CC708Pen        pen;

    /// set to false when DeleteWindow is called on the window.
    bool            exists;
    bool            changed;

    mutable QMutex  lock;
};

class CC708Service
{
  public:
    CC708Service() { current_window = 0; }

  public:
    uint current_window;
    CC708Window windows[8];
};

#endif // _CC708_WINDOW_
