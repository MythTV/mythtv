// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef CC708READER_H
#define CC708READER_H

#include <cstdint>
#include "libmythbase/compat.h"
#include "libmythtv/captions/cc708window.h"
#include "libmythtv/format.h"

class MythPlayer;

const uint k708MaxServices = 64;

class CC708Reader
{
  public:
    explicit CC708Reader(MythPlayer *owner);
    virtual ~CC708Reader();

    void SetCurrentService(int service) { m_currentService = service; }
    CC708Service* GetCurrentService(void) { return &m_cc708services[m_currentService]; }
    void SetEnabled(bool enable) { m_enabled = enable; }
    void ClearBuffers(void);

    CC708Service* GetService(uint service_num)
        { return &(m_cc708services[service_num]); }
    CC708Window &GetCCWin(uint service_num, uint window_id)
        { return m_cc708services[service_num].m_windows[window_id]; }
    CC708Window &GetCCWin(uint svc_num)
        { return GetCCWin(svc_num, m_cc708services[svc_num].m_currentWindow); }

    // Window settings
    virtual void SetCurrentWindow(uint service_num, int window_id);
    virtual void DefineWindow(uint service_num,     int window_id,
                              int priority,         bool visible,
                              int anchor_point,     int relative_pos,
                              int anchor_vertical,  int anchor_horizontal,
                              int row_count,        int column_count,
                              int row_lock,         int column_lock,
                              int pen_style,        int window_style);
    virtual void DeleteWindows( uint service_num,   int window_map);
    virtual void DisplayWindows(uint service_num,   int window_map);
    virtual void HideWindows(   uint service_num,   int window_map);
    virtual void ClearWindows(  uint service_num,   int window_map);
    virtual void ToggleWindows( uint service_num,   int window_map);
    virtual void SetWindowAttributes(uint service_num,
                                     int fill_color,     int fill_opacity,
                                     int border_color,   int border_type,
                                     int scroll_dir,     int print_dir,
                                     int effect_dir,
                                     int display_effect, int effect_speed,
                                     int justify,        int word_wrap);

    // Pen settings
    virtual void SetPenAttributes(uint service_num,
                                  int pen_size,  int offset,
                                  int text_tag,  int font_tag,
                                  int edge_type,
                                  int underline, int italics);
    virtual void SetPenColor(uint service_num,
                             int fg_color, int fg_opacity,
                             int bg_color, int bg_opacity,
                             int edge_color);
    virtual void SetPenLocation(uint service_num, int row, int column);

    // Display State
    virtual void Delay(uint service_num, int tenths_of_seconds);
    virtual void DelayCancel(uint service_num);
    virtual void Reset(uint service_num);

    // Text
    virtual void TextWrite(uint service_num,
                           int16_t* unicode_string, int16_t len);

    // Data
    std::array<unsigned char *,k708MaxServices> m_buf          {};
    std::array<uint,k708MaxServices>            m_bufAlloc     {};
    std::array<uint,k708MaxServices>            m_bufSize      {};
    std::array<bool,k708MaxServices>            m_delayed      {};

    std::array<int16_t *,k708MaxServices>       m_tempStr      {};
    std::array<int,k708MaxServices>             m_tempStrAlloc {};
    std::array<int,k708MaxServices>             m_tempStrSize  {};

    int                                         m_currentService {1};
    std::array<CC708Service,k708MaxServices>    m_cc708services;
    std::array<int,k708MaxServices>             m_cc708DelayedDeletes {};

    MythPlayer    *m_parent  {nullptr};
    bool           m_enabled {false};
};
#endif // CC708READER_H
