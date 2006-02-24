// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson 

#ifndef CC708DECODER_H_
#define CC708DECODER_H_

#include <stdint.h>

#include <qstringlist.h>

#include "format.h"

#ifndef __CC_CALLBACKS_H__
/** EIA-708-A closed caption packet */
typedef struct CaptionPacket
{
    unsigned char data[128+16];
    int size;
} CaptionPacket;
#endif

class CC708Reader
{
  public:
    CC708Reader();
    virtual ~CC708Reader();

    // Window settings
    virtual void SetCurrentWindow(uint service_num, int window_id) = 0;
    virtual void DefineWindow(uint service_num,     int window_id,
                              int priority,         int visible,
                              int anchor_point,     int relative_pos,
                              int anchor_vertical,  int anchor_horizontal,
                              int row_count,        int column_count,
                              int row_lock,         int column_lock,
                              int pen_style,        int window_style) = 0;
    virtual void DeleteWindows( uint service_num,   int window_map) = 0;
    virtual void DisplayWindows(uint service_num,   int window_map) = 0;
    virtual void HideWindows(   uint service_num,   int window_map) = 0;
    virtual void ClearWindows(  uint service_num,   int window_map) = 0;
    virtual void ToggleWindows( uint service_num,   int window_map) = 0;
    virtual void SetWindowAttributes(uint service_num,
                                     int fill_color,     int fill_opacity,
                                     int border_color,   int border_type,
                                     int scroll_dir,     int print_dir,
                                     int effect_dir,
                                     int display_effect, int effect_speed,
                                     int justify,        int word_wrap) = 0;

    // Pen settings
    virtual void SetPenAttributes(uint service_num,
                                  int pen_size,  int offset,
                                  int text_tag,  int font_tag,
                                  int edge_type,
                                  int underline, int italics) = 0;
    virtual void SetPenColor(uint service_num,
                             int fg_color, int fg_opacity,
                             int bg_color, int bg_opacity,
                             int edge_color) = 0;
    virtual void SetPenLocation(uint service_num, int row, int column) = 0;

    // Display State
    virtual void Delay(uint service_num, int tenths_of_seconds) = 0;
    virtual void DelayCancel(uint service_num) = 0;
    virtual void Reset(uint service_num) = 0;

    // Text
    virtual void TextWrite(uint service_num,
                           short* unicode_string, short len) = 0;

    // Data
    unsigned char* buf[64];
    uint   buf_alloc[64];
    uint   buf_size[64];
    bool   delayed[64];

    short* temp_str[64];
    int    temp_str_alloc[64];
    int    temp_str_size[64];
};

class CC708Decoder
{
  public:
    CC708Decoder(CC708Reader *ccr) : reader(ccr)
        { bzero(&partialPacket, sizeof(CaptionPacket)); }

    ~CC708Decoder();

    void decode_cc_data(uint cc_type, uint data1, uint data2);

  private:
    CaptionPacket  partialPacket;
    CC708Reader   *reader;
};

#endif // CC708DECODER_H_
