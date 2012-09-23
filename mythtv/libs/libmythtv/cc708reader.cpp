// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#include <cstdlib>

#include "mythlogging.h"
#include "decoderbase.h"
#include "mythplayer.h"
#include "cc708reader.h"

#define LOC QString("CC708Reader: ")
#define CHECKENABLED if (!enabled) return

CC708Reader::CC708Reader(MythPlayer *owner)
  : currentservice(1), parent(owner), enabled(false)
{
    for (uint i=0; i<64; i++)
    {
        buf_alloc[i] = 512;
        buf[i]       = (unsigned char*) malloc(buf_alloc[i]);
        buf_size[i]  = 0;
        delayed[i]   = 0;

        temp_str_alloc[i] = 512;
        temp_str_size[i]  = 0;
        temp_str[i]       = (short*) malloc(temp_str_alloc[i] * sizeof(short));
    }
    memset(&CC708DelayedDeletes, 0, sizeof(CC708DelayedDeletes));
}

CC708Reader::~CC708Reader()
{
    for (uint i=0; i<64; i++)
    {
        free(buf[i]);
        free(temp_str[i]);
    }
}

void CC708Reader::ClearBuffers(void)
{
    for (uint i = 1; i < 64; i++)
        DeleteWindows(i, 0xff);
}

void CC708Reader::SetCurrentWindow(uint service_num, int window_id)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("SetCurrentWindow(%1, %2)")
            .arg(service_num).arg(window_id));
    CC708services[service_num].current_window = window_id;
}

void CC708Reader::DefineWindow(
    uint service_num,     int window_id,
    int priority,         int visible,
    int anchor_point,     int relative_pos,
    int anchor_vertical,  int anchor_horizontal,
    int row_count,        int column_count,
    int row_lock,         int column_lock,
    int pen_style,        int window_style)
{
    if (parent && parent->GetDecoder())
    {
        StreamInfo si(-1, 0, 0, service_num, false, false);
        parent->GetDecoder()->InsertTrack(kTrackTypeCC708, si);
    }

    CHECKENABLED;

    CC708DelayedDeletes[service_num & 63] &= ~(1 << window_id);

    LOG(VB_VBI, LOG_INFO, LOC +
        QString("DefineWindow(%1, %2,\n\t\t\t\t\t")
            .arg(service_num).arg(window_id) +
        QString("  prio %1, vis %2, ap %3, rp %4, av %5, ah %6")
            .arg(priority).arg(visible).arg(anchor_point).arg(relative_pos)
            .arg(anchor_vertical).arg(anchor_horizontal) +
        QString("\n\t\t\t\t\t  row_cnt %1, row_lck %2, "
                    "col_cnt %3, col_lck %4 ")
            .arg(row_count).arg(row_lock)
            .arg(column_count).arg(column_lock) +
        QString("\n\t\t\t\t\t  pen style %1, win style %2)")
            .arg(pen_style).arg(window_style));

    GetCCWin(service_num, window_id)
        .DefineWindow(priority,         visible,
                      anchor_point,     relative_pos,
                      anchor_vertical,  anchor_horizontal,
                      row_count,        column_count,
                      row_lock,         column_lock,
                      pen_style,        window_style);

    CC708services[service_num].current_window = window_id;
}

void CC708Reader::DeleteWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("DeleteWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
        if ((1 << i) & window_map)
            GetCCWin(service_num, i).Clear();
    CC708DelayedDeletes[service_num&63] |= window_map;
}

void CC708Reader::DisplayWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("DisplayWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & CC708DelayedDeletes[service_num & 63])
        {
            CC708Window &win = GetCCWin(service_num, i);
            QMutexLocker locker(&win.lock);

            win.exists  = false;
            win.changed = true;
            if (win.text)
            {
                delete [] win.text;
                win.text = NULL;
            }
        }
        CC708DelayedDeletes[service_num & 63] = 0;
    }

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i ) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.visible = true;
            win.changed = true;
            LOG(VB_VBI, LOG_INFO, LOC +
                QString("DisplayedWindow(%1, %2)").arg(service_num).arg(i));
        }
    }
}

void CC708Reader::HideWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("HideWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.visible = false;
            win.changed = true;
        }
    }
}

void CC708Reader::ClearWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("ClearWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
        if ((1 << i) & window_map)
            GetCCWin(service_num, i).Clear();
}

void CC708Reader::ToggleWindows(uint service_num, int window_map)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("ToggleWindows(%1, %2)")
            .arg(service_num).arg(window_map, 8, 2, QChar(48)));

    for (uint i = 0; i < 8; i++)
    {
        if ((1 << i) & window_map)
        {
            CC708Window &win = GetCCWin(service_num, i);
            win.visible = !win.visible;
            win.changed = true;
        }
    }
}

void CC708Reader::SetWindowAttributes(
    uint service_num,
    int fill_color,     int fill_opacity,
    int border_color,   int border_type,
    int scroll_dir,     int print_dir,
    int effect_dir,
    int display_effect, int effect_speed,
    int justify,        int word_wrap)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("SetWindowAttributes(%1...)")
            .arg(service_num));

    CC708Window &win = GetCCWin(service_num);

    win.fill_color     = fill_color   & 0x3f;
    win.fill_opacity   = fill_opacity;
    win.border_color   = border_color & 0x3f;
    win.border_type    = border_type;
    win.scroll_dir     = scroll_dir;
    win.print_dir      = print_dir;
    win.effect_dir     = effect_dir;
    win.display_effect = display_effect;
    win.effect_speed   = effect_speed;
    win.justify        = justify;
    win.word_wrap      = word_wrap;
}

void CC708Reader::SetPenAttributes(
    uint service_num, int pen_size,
    int offset,       int text_tag,  int font_tag,
    int edge_type,    int underline, int italics)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("SetPenAttributes(%1, %2,")
            .arg(service_num).arg(CC708services[service_num].current_window) +
            QString("\n\t\t\t\t\t      pen_size %1, offset %2, text_tag %3, "
                    "font_tag %4,"
                    "\n\t\t\t\t\t      edge_type %5, underline %6, italics %7")
            .arg(pen_size).arg(offset).arg(text_tag).arg(font_tag)
            .arg(edge_type).arg(underline).arg(italics));

    GetCCWin(service_num).pen.SetAttributes(
        pen_size, offset, text_tag, font_tag, edge_type, underline, italics);
}

void CC708Reader::SetPenColor(
    uint service_num,
    int fg_color, int fg_opacity,
    int bg_color, int bg_opacity,
    int edge_color)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("SetPenColor(%1...)")
            .arg(service_num));

    CC708CharacterAttribute &attr = GetCCWin(service_num).pen.attr;

    attr.fg_color   = fg_color;
    attr.fg_opacity = fg_opacity;
    attr.bg_color   = bg_color;
    attr.bg_opacity = bg_opacity;
    attr.edge_color = edge_color;
}

void CC708Reader::SetPenLocation(uint service_num, int row, int column)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("SetPenLocation(%1, (c %2, r %3))")
            .arg(service_num).arg(column).arg(row));
    GetCCWin(service_num).SetPenLocation(row, column);
}

void CC708Reader::Delay(uint service_num, int tenths_of_seconds)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("Delay(%1, %2 seconds)")
            .arg(service_num).arg(tenths_of_seconds * 0.1f));
}

void CC708Reader::DelayCancel(uint service_num)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("DelayCancel(%1)").arg(service_num));
}

void CC708Reader::Reset(uint service_num)
{
    CHECKENABLED;
    LOG(VB_VBI, LOG_INFO, LOC + QString("Reset(%1)").arg(service_num));
    DeleteWindows(service_num, 0x7);
    DelayCancel(service_num);
}

void CC708Reader::TextWrite(uint service_num,
                                  short* unicode_string, short len)
{
    CHECKENABLED;
    QString debug = QString();
    for (uint i = 0; i < (uint)len; i++)
    {
        GetCCWin(service_num).AddChar(QChar(unicode_string[i]));
        debug += QChar(unicode_string[i]);
    }
    LOG(VB_VBI, LOG_INFO, LOC + QString("AddText to %1->%2 |%3|")
        .arg(service_num).arg(CC708services[service_num].current_window).arg(debug));
}

void CC708Reader::SetOSDFontName(const QString osdfonts[22],
                                       const QString &prefix)
{
    osdfontname   = osdfonts[0]; osdfontname.detach();
    osdccfontname = osdfonts[1]; osdccfontname.detach();
    for (int i = 2; i < 22; i++)
    {
        QString tmp = osdfonts[i]; tmp.detach();
        osd708fontnames[i - 2] = tmp;
    }
    osdprefix = prefix; osdprefix.detach();
}

void CC708Reader::SetOSDThemeName(const QString themename)
{
    osdtheme = themename; osdtheme.detach();
}
