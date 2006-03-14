// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson 

#ifndef _CC708_WINDOW_
#define _CC708_WINDOW_

#include <vector>
using namespace std;

#include <qstring.h>
#include <qmutex.h>
#include <qcolor.h>

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

    uint fg_color;
    uint fg_opacity;
    uint bg_color;
    uint bg_opacity;
    uint edge_color;

    uint FontIndex(void) const
    {
        return (((font_tag & 0x7) * 6) + ((italics) ? 3 : 0) +
                (pen_size & 0x3));
    }
    static QColor ConvertToQColor(uint eia708color);
    QColor GetFGColor(void) const { return ConvertToQColor(fg_color); }
    QColor GetBGColor(void) const { return ConvertToQColor(bg_color); }
    QColor GetEdgeColor(void) const { return ConvertToQColor(edge_color); }

    uint GetFGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0xc0, 0x60, };
        return alpha[fg_opacity & 0x3];
    }

    uint GetBGAlpha(void) const
    {
        //SOLID=0, FLASH=1, TRANSLUCENT=2, and TRANSPARENT=3.
        static uint alpha[4] = { 0xff, 0xff, 0xc0, 0x60, };
        return alpha[bg_opacity & 0x3];
    }

    bool operator==(const CC708CharacterAttribute &other) const;
    bool operator!=(const CC708CharacterAttribute &other) const
        { return !(*this == other); }
};

class CC708Pen
{
  public:
    void SetPenStyle(uint style);
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

class CC708Window
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
    void Clear(void);
    void SetWindowStyle(uint);

    void AddChar(QChar);
    void IncrPenLocation(void);
    void SetPenLocation(uint, uint);
    void LimitPenLocation(void);

    bool IsPenValid(void) const
    {
        return ((pen.row < true_row_count) &&
                (pen.column < true_column_count));
    }
    CC708Character &GetCCChar(void) const;
    vector<CC708String*> GetStrings(void) const;

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

    uint true_row_count;
    uint true_column_count;
    CC708Character *text;
    CC708Pen        pen;

    /// set to false when DeleteWindow is called on the window.
    bool            exists;

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
