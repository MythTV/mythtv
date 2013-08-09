// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#include <cstdlib>
#include <stdio.h>

#include "mythlogging.h"
#include "cc708reader.h"
#include "cc708decoder.h"

#define LOC QString("CC708: ")

#define DEBUG_CAPTIONS         0
#define DEBUG_CC_SERVICE       0
#define DEBUG_CC_SERVICE_2     0
#define DEBUG_CC_RAWPACKET     0
#define DEBUG_CC_VALIDPACKET   0
#define DEBUG_CC_SERVICE_BLOCK 0

typedef enum
{
    NTSC_CC_f1         = 0,
    NTSC_CC_f2         = 1,
    DTVCC_PACKET_DATA  = 2,
    DTVCC_PACKET_START = 3,
} kCCTypes;

const char* cc_types[4] =
{
    "NTSC line 21 field 1 closed captions"
    "NTSC line 21 field 2 closed captions"
    "DTVCC Channel Packet Data"
    "DTVCC Channel Packet Start"
};

static void parse_cc_packet(CC708Reader *cb_cbs, CaptionPacket *pkt,
                            time_t last_seen[64]);

void CC708Decoder::decode_cc_data(uint cc_type, uint data1, uint data2)
{
    if (DTVCC_PACKET_START == cc_type)
    {
#if 0
        LOG(VB_VBI, LOG_DEBUG, LOC + QString("CC ST data(0x%1 0x%2)")
                .arg(data1,0,16).arg(data2,0,16));
#endif

        if (partialPacket.size && reader)
            parse_cc_packet(reader, &partialPacket, last_seen);

        partialPacket.data[0] = data1;
        partialPacket.data[1] = data2;
        partialPacket.size   = 2;
    }
    else if (DTVCC_PACKET_DATA == cc_type)
    {
#if 0
        LOG(VB_VBI, LOG_DEBUG, LOC + QString("CC Ex data(0x%1 0x%2)")
                .arg(data1,0,16).arg(data2,0,16));
#endif

        partialPacket.data[partialPacket.size + 0] = data1;
        partialPacket.data[partialPacket.size + 1] = data2;
        partialPacket.size += 2;
    }
}

void CC708Decoder::decode_cc_null(void)
{
    if (partialPacket.size && reader)
        parse_cc_packet(reader, &partialPacket, last_seen);
    partialPacket.size = 0;
}

void CC708Decoder::services(uint seconds, bool seen[64]) const
{
    time_t now = time(NULL);
    time_t then = now - seconds;

    seen[0] = false; // service zero is not allowed in CEA-708-D
    for (uint i = 1; i < 64; i++)
        seen[i] = (last_seen[i] >= then);
}

typedef enum
{
    NUL  = 0x00,
    ETX  = 0x03,
    BS   = 0x08,
    FF   = 0x0C,
    CR   = 0x0D,
    HCR  = 0x0E,
    EXT1 = 0x10,
    P16  = 0x18,
} C0;

typedef enum
{
    CW0=0x80, CW1, CW2, CW3, CW4, CW5, CW6, CW7,
    CLW,      DSW, HDW, TGW, DLW, DLY, DLC, RST,
    SPA=0x90, SPC, SPL,                     SWA=0x97,
    DF0,      DF1, DF2, DF3, DF4, DF5, DF6, DF7,
} C1;

extern ushort CCtableG0[0x60];
extern ushort CCtableG1[0x60];
extern ushort CCtableG2[0x60];
extern ushort CCtableG3[0x60];

static void append_character(CC708Reader *cc, uint service_num, short ch);
static void parse_cc_service_stream(CC708Reader *cc, uint service_num);
static int handle_cc_c0_ext1_p16(CC708Reader *cc, uint service_num, int i);
static int handle_cc_c1(CC708Reader *cc, uint service_num, int i);
static int handle_cc_c2(CC708Reader *cc, uint service_num, int i);
static int handle_cc_c3(CC708Reader *cc, uint service_num, int i);

#define SEND_STR \
do { \
    if (cc->temp_str_size[service_num]) \
    { \
        cc->TextWrite(service_num, \
                      cc->temp_str[service_num], \
                      cc->temp_str_size[service_num]); \
        cc->temp_str_size[service_num] = 0; \
    } \
} while (0)

static void parse_cc_service_stream(CC708Reader* cc, uint service_num)
{
    const int blk_size = cc->buf_size[service_num];
    int blk_start = 0, dlc_loc = 0, rst_loc = 0, i = 0;

    // find last reset or delay cancel in buffer
    for (i = 0; i < blk_size; i++)
    {
        switch (cc->buf[service_num][i]) {
            // Skip over parameters, since their bytes may coincide
            // with RST or DLC
            case CLW:
            case DLW:
            case DSW:
            case HDW:
            case TGW:
            case DLY:
                i += 1;
                break;
            case SPA:
            case SPL:
                i += 2;
                break;
            case SPC:
                i += 3;
                break;
            case SWA:
                i += 4;
                break;
            case DF0:
            case DF1:
            case DF2:
            case DF3:
            case DF4:
            case DF5:
            case DF6:
            case DF7:
                i += 6;
                break;
            // Detect RST or DLC bytes
            case RST:
                rst_loc = dlc_loc = i;
                break;
            case DLC:
                dlc_loc = i;
                break;
        }
    }

    // reset, process only data after reset
    if (rst_loc)
    {
        cc->Reset(service_num);
        cc->delayed[service_num] = 0; // Reset implicitly cancels delay
        blk_start = rst_loc + 1;
    }

    // if we have a delay cancel, cancel any delay
    if (dlc_loc && cc->delayed[service_num])
    {
        cc->DelayCancel(service_num);
        cc->delayed[service_num] = 0;
    }

    // cancel delay if the buffer is full
    if (cc->delayed[service_num] && blk_size >= 126)
    {
        cc->DelayCancel(service_num);
        cc->delayed[service_num] = 0;
        dlc_loc = blk_size - 1;
    }

#if 0
    LOG(VB_VBI, LOG_ERR,
        QString("cc_ss delayed(%1) blk_start(%2) blk_size(%3)")
            .arg(cc->delayed) .arg(blk_start) .arg(blk_size));
#endif

    for (i = (cc->delayed[service_num]) ? blk_size : blk_start;
         i < blk_size; )
    {
        const int old_i = i;
        const int code = cc->buf[service_num][i];
        if (0x0 == code)
        {
            i++;
        }
        else if (code <= 0x1f)
        {
            // C0 code -- ASCII commands + ext1: C2,C3,G2,G3 + p16: 16 chars
            i = handle_cc_c0_ext1_p16(cc, service_num, i);
        }
        else if (code <= 0x7f)
        {
            // G0 code -- mostly ASCII printables
            short character = CCtableG0[code-0x20];
            append_character(cc, service_num, character);
            i++;
            SEND_STR;
        }
        else if (code <= 0x9f)
        {
            // C1 code -- caption control codes
            i = handle_cc_c1(cc, service_num, i);
        }
        else if (code <= 0xff)
        {
            // G1 code -- ISO 8859-1 Latin 1 characters
            short character = CCtableG1[code-0xA0];
            append_character(cc, service_num, character);
            i++;
        }

#if DEBUG_CC_SERVICE
        LOG(VB_VBI, LOG_DEBUG, QString("i %1, blk_size %2").arg(i)
                .arg(blk_size));
#endif

        // loop continuation check
        if (old_i == i)
        {
#if DEBUG_CC_SERVICE
            LOG(VB_VBI, LOG_DEBUG, QString("old_i == i == %1").arg(i));
            QString msg;
            for (int j = 0; j < blk_size; j++)
                msg += QString("0x%1 ").arg(cc->buf[service_num][j], 0, 16);
            LOG(VB_VBI, LOG_DEBUG, msg);
#endif
            if (blk_size - i > 10)
            {
                LOG(VB_VBI, LOG_INFO, "eia-708 decoding error...");
                cc->Reset(service_num);
                cc->delayed[service_num] = 0;
                i = cc->buf_size[service_num];
            }
            // There must be an incomplete code in buffer...
            break;
        }
        else if (cc->delayed[service_num] && dlc_loc < i)
        {
            // delay in effect
            break;
        }
        else if (cc->delayed[service_num])
        {
            // this delay has already been canceled..
            cc->DelayCancel(service_num);
            cc->delayed[service_num] = 0;
        }
    }

    // get rid of remaining bytes...
    if ((blk_size - i) > 0)
    {
        memmove(cc->buf[service_num], cc->buf[service_num] + i,
                blk_size - i);
        cc->buf_size[service_num] -= i;
    }
    else
    {
        if (0 != (blk_size - i))
        {
            LOG(VB_VBI, LOG_ERR, QString("buffer error i(%1) buf_size(%2)")
                .arg(i).arg(blk_size));
            QString msg;
            for (i=0; i < blk_size; i++)
                msg += QString("0x%1 ").arg(cc->buf[service_num][i], 0, 16);
            LOG(VB_VBI, LOG_ERR, msg);
        }
        cc->buf_size[service_num] = 0;
    }
}

static int handle_cc_c0_ext1_p16(CC708Reader* cc, uint service_num, int i)
{
    // C0 code -- subset of ASCII misc. control codes
    const int code = cc->buf[service_num][i];
    if (code<=0xf)
    {
        // single byte code
        if (ETX==code)
            SEND_STR;
        else if (BS==code)
            append_character(cc, service_num, 0x08);
        else if (FF==code)
            append_character(cc, service_num, 0x0c);
        else if (CR==code)
            append_character(cc, service_num, 0x0d);
        else if (HCR==code)
            append_character(cc, service_num, 0x0d);
        i++;
    }
    else if (code<=0x17)
    {
        // double byte code
        const int blk_size = cc->buf_size[service_num];
        if (EXT1==code && ((i+1)<blk_size))
        {
            const int code2 = cc->buf[service_num][i+1];
            if (code2<=0x1f)
            {
                // C2 code -- nothing in EIA-708-A
                i = handle_cc_c2(cc, service_num, i+1);
            }
            else if (code2<=0x7f)
            {
                // G2 code -- fractions, drawing, symbols
                append_character(cc, service_num, CCtableG2[code2-0x20]);
                i+=2;
            }
            else if (code2<=0x9f)
            {
                // C3 code -- nothing in EIA-708-A
                i = handle_cc_c3(cc, service_num, i);
            }
            else if (code2<=0xff)
            {
                // G3 code -- one symbol in EIA-708-A "[cc]"
                append_character(cc, service_num, CCtableG3[code2-0xA0]);
                i+=2;
            }
        }
        else if ((i+1)<blk_size)
            i+=2;
    }
    else if (code<=0x1f)
    {
        // triple byte code
        const int blk_size = cc->buf_size[service_num];
        if (P16==code && ((i+2)<blk_size))
        {
            // reserved for large alphabets, but not yet defined
        }
        if ((i+2)<blk_size)
            i+=3;
    }
    return i;
}

static int handle_cc_c1(CC708Reader* cc, uint service_num, int i)
{
    const int blk_size = cc->buf_size[service_num];
    const int code = cc->buf[service_num][i];

    const unsigned char* blk_buf = cc->buf[service_num];
    if (code<=CW7)
    { // no paramaters
        SEND_STR;
        cc->SetCurrentWindow(service_num, code-0x80);
        i+=1;
    }
    else if (DLC == cc->buf[service_num][i])
    {
/* processed out-of-band
        cc->DelayCancel(service_num);
        cc->delayed[service_num] = 0;
*/
        i+=1;
    }
    else if (code>=CLW && code<=DLY && ((i+1)<blk_size))
    { // 1 byte of paramaters
        int param1 = blk_buf[i+1];
        SEND_STR;
        if (CLW==code)
            cc->ClearWindows(service_num, param1);
        else if (DSW==code)
            cc->DisplayWindows(service_num, param1);
        else if (HDW==code)
            cc->HideWindows(service_num, param1);
        else if (TGW==code)
            cc->ToggleWindows(service_num, param1);
        else if (DLW==code)
            cc->DeleteWindows(service_num, param1);
        else if (DLY==code)
        {
            cc->Delay(service_num, param1);
            cc->delayed[service_num] = 1;
        }
        i+=2;
    }
    else if (SPA==code && ((i+2)<blk_size))
    {
        int pen_size  = (blk_buf[i+1]   ) & 0x3;
        int offset    = (blk_buf[i+1]>>2) & 0x3;
        int text_tag  = (blk_buf[i+1]>>4) & 0xf;
        int font_tag  = (blk_buf[i+2]   ) & 0x7;
        int edge_type = (blk_buf[i+2]>>3) & 0x7;
        int underline = (blk_buf[i+2]>>6) & 0x1;
        int italic    = (blk_buf[i+2]>>7) & 0x1;
        SEND_STR;
        cc->SetPenAttributes(service_num, pen_size, offset, text_tag,
                             font_tag, edge_type, underline, italic);
        i+=3;
    }
    else if (SPC==code && ((i+3)<blk_size))
    {
        int fg_color   = (blk_buf[i+1]   ) & 0x3f;
        int fg_opacity = (blk_buf[i+1]>>6) & 0x03;
        int bg_color   = (blk_buf[i+2]   ) & 0x3f;
        int bg_opacity = (blk_buf[i+2]>>6) & 0x03;
        int edge_color = (blk_buf[i+3]>>6) & 0x3f;
        SEND_STR;
        cc->SetPenColor(service_num, fg_color, fg_opacity,
                        bg_color, bg_opacity, edge_color);
        i+=4;
    }
    else if (SPL==code && ((i+2)<blk_size))
    {
        int row = blk_buf[i+1] & 0x0f;
        int col = blk_buf[i+2] & 0x3f;
        SEND_STR;
        cc->SetPenLocation(service_num, row, col);
        i+=3;
    }
    else if (SWA==code && ((i+4)<blk_size))
    {
        int fill_color    = (blk_buf[i+1]   ) & 0x3f;
        int fill_opacity  = (blk_buf[i+1]>>6) & 0x03;
        int border_color  = (blk_buf[i+2]   ) & 0x3f;
        int border_type01 = (blk_buf[i+2]>>6) & 0x03;
        int justify       = (blk_buf[i+3]   ) & 0x03;
        int scroll_dir    = (blk_buf[i+3]>>2) & 0x03;
        int print_dir     = (blk_buf[i+3]>>4) & 0x03;
        int word_wrap     = (blk_buf[i+3]>>6) & 0x01;
        int border_type   = (blk_buf[i+3]>>5) | border_type01;
        int display_eff   = (blk_buf[i+4]   ) & 0x03;
        int effect_dir    = (blk_buf[i+4]>>2) & 0x03;
        int effect_speed  = (blk_buf[i+4]>>4) & 0x0f;
        SEND_STR;
        cc->SetWindowAttributes(
            service_num, fill_color, fill_opacity, border_color, border_type,
            scroll_dir,     print_dir,    effect_dir,
            display_eff,    effect_speed, justify, word_wrap);
        i+=5;
    }
    else if ((code>=DF0) && (code<=DF7) && ((i+6)<blk_size))
    {
        // param1
        int priority = ( blk_buf[i+1]  ) & 0x7;
        int col_lock = (blk_buf[i+1]>>3) & 0x1;
        int row_lock = (blk_buf[i+1]>>4) & 0x1;
        int visible  = (blk_buf[i+1]>>5) & 0x1;
        // param2
        int anchor_vertical = blk_buf[i+2] & 0x7f;
        int relative_pos = (blk_buf[i+2]>>7);
        // param3
        int anchor_horizontal = blk_buf[i+3];
        // param4
        int row_count = blk_buf[i+4] & 0xf;
        int anchor_point = blk_buf[i+4]>>4;
        // param5
        int col_count = blk_buf[i+5] & 0x3f;
        // param6
        int pen_style = blk_buf[i+6] & 0x7;
        int win_style = (blk_buf[i+6]>>3) & 0x7;
        SEND_STR;
        cc->DefineWindow(service_num, code-0x98, priority, visible,
                         anchor_point, relative_pos,
                         anchor_vertical, anchor_horizontal,
                         row_count, col_count, row_lock, col_lock,
                         pen_style, win_style);
        i+=7;
    }
#if DEBUG_CC_SERVICE
    else
    {
        LOG(VB_VBI, LOG_ERR, QString("handle_cc_c1: (NOT HANDLED) "
                "code(0x%1) i(%2) blk_size(%3)").arg(code, 2, 16, '0')
                .arg(i).arg(blk_size));
    }
#endif

    return i;
}

static int handle_cc_c2(CC708Reader* cc, uint service_num, int i)
{
    const int blk_size = cc->buf_size[service_num];
    const int code = cc->buf[service_num][i+1];

    if ((code<=0x7) && ((i+1)<blk_size)){
        i+=2;
        SEND_STR;
    }
    else if ((code<=0xf) && ((i+2)<blk_size))
    {
        i+=3;
        SEND_STR;
    }
    else if ((code<=0x17) && ((i+3)<blk_size))
    {
        i+=4;
        SEND_STR;
    }
    else if ((code<=0x1f) && ((i+4)<blk_size))
    {
        i+=5;
        SEND_STR;
    }
    return i;
}

static int handle_cc_c3(CC708Reader* cc, uint service_num, int i)
{
    const unsigned char* blk_buf = cc->buf[service_num];
    const int blk_size = cc->buf_size[service_num];
    const int code = cc->buf[service_num][i+1];

    if ((code<=0x87) && ((i+5)<blk_size))
    {
        i+=6;
        SEND_STR;
    }
    else if ((code<=0x8f) && ((i+6)<blk_size))
    {
        i+=7;
        SEND_STR;
    }
    else if ((i+2)<blk_size)
    { // varible length commands
        int length = blk_buf[i+2]&0x3f;
        if ((i+length)<blk_size)
        {
            i+=1+length;
            SEND_STR;
        }
    }
    return i;
}

static void rightsize_buf(CC708Reader* cc, uint service_num, uint block_size)
{
    uint min_new_size = block_size + cc->buf_size[service_num];
    if (min_new_size >= cc->buf_alloc[service_num])
    {
        uint new_alloc    = cc->buf_alloc[service_num];
        for (uint i = 0; (i < 32) && (new_alloc <= min_new_size); i++)
            new_alloc *= 2;

        cc->buf[service_num] =
            (unsigned char*) realloc(cc->buf[service_num], new_alloc);
        cc->buf_alloc[service_num] = (cc->buf[service_num]) ? new_alloc : 0;

#if DEBUG_CC_SERVICE_2
        LOG(VB_VBI, LOG_DEBUG, QString("rightsize_buf: srv %1 to %1 bytes")
                .arg(service_num) .arg(cc->buf_alloc[service_num]));
#endif
    }
    if (min_new_size >= cc->buf_alloc[service_num])
        LOG(VB_VBI, LOG_ERR,
            QString("buffer resize error: min_new_size=%1, buf_alloc[%2]=%3")
            .arg(min_new_size)
            .arg(service_num)
            .arg(cc->buf_alloc[service_num]));
}

static void append_cc(CC708Reader* cc, uint service_num,
                      const unsigned char* blk_buf, int block_size)
{
    rightsize_buf(cc, service_num, block_size);

    memcpy(cc->buf[service_num] + cc->buf_size[service_num],
           blk_buf, block_size);

    cc->buf_size[service_num] += block_size;
#if DEBUG_CC_SERVICE_2
    {
        uint i;
        QString msg("append_cc: ");
        for (i = 0; i < cc->buf_size[service_num]; i++)
            msg += QString("0x%1").arg(cc->buf[service_num][i], 0, 16);
        LOG(VB_VBI, LOG_DEBUG, msg);
    }
#endif
    parse_cc_service_stream(cc, service_num);
}

static void parse_cc_packet(CC708Reader* cb_cbs, CaptionPacket* pkt,
                            time_t last_seen[64])
{
    const unsigned char* pkt_buf = pkt->data;
    const int pkt_size = pkt->size;
    int off = 1;
    int service_number = 0;
    int block_data_offset = 0;
    int len     = ((((int)pkt_buf[0]) & 0x3f)<<1) - 1;

    if (len < 0)
        return;

#if DEBUG_CC_RAWPACKET
    if (1)
#elif DEBUG_CAPTIONS
    if (len > pkt_size)
#else
    if (0)
#endif
    {
        int j;
        int srv = (pkt_buf[off]>>5) & 0x7;
        int seq_num = (((int)pkt_buf[0])>>6)&0x3;
        QString msg = QString("CC708 len %1 srv0 %2 seq %3 ").arg(len, 2)
                          .arg(srv) .arg(seq_num);
        for (j = 0; j < pkt_size; j++)
            msg += QString("0x%1").arg(pkt_buf[j], 0, 16);
        LOG(VB_VBI, LOG_DEBUG, msg);
    }

    if (pkt_size >= 127)
        LOG(VB_VBI, LOG_ERR,
            QString("Unexpected pkt_size=%1").arg(pkt_size));

    while (off < pkt_size && pkt_buf[off])
    { // service_block
        int block_size = pkt_buf[off] & 0x1f;
        service_number = (pkt_buf[off]>>5) & 0x7;
        block_data_offset = (0x7==service_number && block_size!=0) ?
            off+2 : off+1;
#if DEBUG_CC_SERVICE_BLOCK
        LOG(VB_VBI, LOG_DEBUG, 
            QString("service_block size(%1) num(%2) off(%3)")
                .arg(block_size) .arg(service_number) .arg(block_data_offset));
#endif
        if (off+2 == block_data_offset)
        {
            int extended_service_number = pkt_buf[off+2] & 0x3f;
#if DEBUG_CC_SERVICE_BLOCK
            LOG(VB_VBI, LOG_DEBUG, QString("ext_svc_num(%1)")
                   .arg(extended_service_number));
#endif
            service_number =  extended_service_number;
        }
        if (service_number)
        {
#if DEBUG_CC_SERVICE
            int i;
            if (!(2==block_size &&
                  0==pkt_buf[block_data_offset] &&
                  0==pkt_buf[block_data_offset+1]))
            {
                QString msg = QString("service %1: ").arg(service_number);
                for (i=0; i<block_size; i++)
                    msg += QString("0x%1 ")
                               .arg(pkt_buf[block_data_offset+i], 0, 16);
                LOG(VB_VBI, LOG_DEBUG, msg);
            }
#endif
            append_cc(cb_cbs, service_number,
                      &pkt_buf[block_data_offset], block_size);

            last_seen[service_number] = time(NULL);
        }
        off+=block_size+1;
    }
    if (off<pkt_size) // must end in null service block, if packet is not full.
    {
        if (pkt_buf[off] != 0)
            LOG(VB_VBI, LOG_ERR,
                QString("CEA-708 packet error: pkt_size=%1, pkt_buf[%2]=%3")
                .arg(pkt_size).arg(off).arg(pkt_buf[off]));
    }
}

static void append_character(CC708Reader *cc, uint service_num, short ch)
{
    if (cc->temp_str_size[service_num]+2 > cc->temp_str_alloc[service_num])
    {
        int new_alloc = (cc->temp_str_alloc[service_num]) ?
            cc->temp_str_alloc[service_num] * 2 : 64;

        cc->temp_str[service_num] = (short*)
            realloc(cc->temp_str[service_num], new_alloc * sizeof(short));

        cc->temp_str_alloc[service_num] = new_alloc; // shorts allocated
    }

    if (cc->temp_str[service_num])
    {
        int i = cc->temp_str_size[service_num];
        cc->temp_str[service_num][i] = ch;
        cc->temp_str_size[service_num]++;
    }
    else
    {
        cc->temp_str_size[service_num] = 0;
        cc->temp_str_alloc[service_num]=0;
    }
}

ushort CCtableG0[0x60] =
{
//   0    1    2    3       4    5    6    7
//   8    9    a    b       c    d    e    f
    ' ', '!','\"', '#',    '$', '%', '&', '\'', /* 0x20-0x27 */
    '(', ')', '*', '+',    ',', '-', '.', '/',  /* 0x28-0x2f */
    '0', '1', '2', '3',    '4', '5', '6', '7',  /* 0x30-0x37 */
    '8', '9', ':', ';',    '<', '=', '>', '?',  /* 0x38-0x3f */

    '@', 'A', 'B', 'C',    'D', 'E', 'F', 'G',  /* 0x40-0x47 */
    'H', 'I', 'J', 'K',    'L', 'M', 'N', 'O',  /* 0x48-0x4f */
    'P', 'Q', 'R', 'S',    'T', 'U', 'V', 'W',  /* 0x50-0x57 */
    'X', 'Y', 'Z', '[',    '\\',']', '^', '_',  /* 0x58-0x5f */

    '`', 'a', 'b', 'c',    'd', 'e', 'f', 'g',  /* 0x60-0x67 */
    'h', 'i', 'j', 'k',    'l', 'm', 'n', 'o',  /* 0x68-0x6f */
    'p', 'q', 'r', 's',    't', 'u', 'v', 'w',  /* 0x70-0x77 */
    'x', 'y', 'z', '{',    '|', '}', '~',  0x266a, // music note/* 0x78-0x7f */
};

ushort CCtableG1[0x60] =
{
//          0           1           2           3
//          4           5           6           7
//          8           9           a           b
//          c           d           e           f
    0xA0, // unicode non-breaking space
                (uchar)'¡', (uchar)'¢', (uchar)'£', /* 0xa0-0xa3 */
    (uchar)'¤', (uchar)'¥', (uchar)'¦', (uchar)'§', /* 0xa4-0xa7 */
    (uchar)'¨', (uchar)'©', (uchar)'ª', (uchar)'«', /* 0xa8-0xab */
    (uchar)'¬', (uchar)'­', (uchar)'®', (uchar)'¯', /* 0xac-0xaf */
    (uchar)'°', (uchar)'±', (uchar)'²', (uchar)'³', /* 0xb0-0xb3 */
    (uchar)'´', (uchar)'µ', (uchar)'¶', (uchar)'·', /* 0xb4-0xb7 */
    (uchar)'¸', (uchar)'¹', (uchar)'º', (uchar)'»', /* 0xb8-0xbb */
    (uchar)'¼', (uchar)'½', (uchar)'¾', (uchar)'¿', /* 0xbc-0xbf */

    (uchar)'À', (uchar)'Á', (uchar)'Â', (uchar)'Ã', /* 0xc0-0xc3 */
    (uchar)'Ä', (uchar)'Å', (uchar)'Æ', (uchar)'Ç', /* 0xc4-0xc7 */
    (uchar)'È', (uchar)'É', (uchar)'Ê', (uchar)'Ë', /* 0xc8-0xcb */
    (uchar)'Ì', (uchar)'Í', (uchar)'Î', (uchar)'Ï', /* 0xcc-0xcf */
    (uchar)'Ð', (uchar)'Ñ', (uchar)'Ò', (uchar)'Ó', /* 0xd0-0xd3 */
    (uchar)'Ô', (uchar)'Õ', (uchar)'Ö', (uchar)'×', /* 0xd4-0xd7 */
    (uchar)'Ø', (uchar)'Ù', (uchar)'Ú', (uchar)'Û', /* 0xd8-0xdb */
    (uchar)'Ü', (uchar)'Ý', (uchar)'Þ', (uchar)'ß', /* 0xdc-0xdf */

    (uchar)'à', (uchar)'á', (uchar)'â', (uchar)'ã', /* 0xe0-0xe3 */
    (uchar)'ä', (uchar)'å', (uchar)'æ', (uchar)'ç', /* 0xe4-0xe7 */
    (uchar)'è', (uchar)'é', (uchar)'ê', (uchar)'ë', /* 0xe8-0xeb */
    (uchar)'ì', (uchar)'í', (uchar)'î', (uchar)'ï', /* 0xec-0xef */
    (uchar)'ð', (uchar)'ñ', (uchar)'ò', (uchar)'ó', /* 0xf0-0xf3 */
    (uchar)'ô', (uchar)'õ', (uchar)'ö', (uchar)'÷', /* 0xf4-0xf7 */
    (uchar)'ø', (uchar)'ù', (uchar)'ú', (uchar)'û', /* 0xf8-0xfb */
    (uchar)'ü', (uchar)'ý', (uchar)'þ', (uchar)'ÿ', /* 0xfc-0xff */
};

ushort CCtableG2[0x60] =
{
    ' ', /* transparent space */
                        0xA0, /* non-breaking transparent space */
    0,                  0,                     /* 0x20-0x23 */
    0,                  0x2026,/* elipsis */
    0,                  0,                     /* 0x24-0x27 */
    0,                  0,
    0x160,/*S under \/*/0,                     /* 0x28-0x2b */
    0x152, /* CE */     0,
    0,                  0,                     /* 0x2c-0x2f */
    0x2588,/*block*/    0x2018,/* open ' */
    0x2019,/*close ' */ 0x201c,/* open " */    /* 0x30-0x33 */
    0x201d,/*close " */ 0xB7,/* dot */
    0,                  0,                     /* 0x34-0x37 */
    0,                  0x2122,/* super TM */
    0x161,/*s under \/*/0,                     /* 0x38-0x3b */
    0x153, /* ce */     0x2120,/* super SM */
    0,                  0x178,/*Y w/umlout*/   /* 0x3c-0x3f */

//  0         1         2         3
//  4         5         6         7
//  8         9         a         b
//  c         d         e         f
    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x40-0x47 */
    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x48-0x4f */

    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x50-0x57 */
    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x58-0x5f */

    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x60-0x67 */
    0,        0,        0,        0,
    0,        0,        0,        0, /* 0x68-0x6f */

    0,                  0,
    0,                  0,           /* 0x70-0x73 */
    0,                  0,
    0x215b, /* 1/8 */   0x215c, /* 3/8 */    /* 0x74-0x77 */
    0x215d, /* 5/8 */   0x215e, /* 7/8 */
    0x2502, /*line | */ 0x2510, /*line ~| */ /* 0x78-0x7b */
    0x2514, /*line |_*/ 0x2500, /*line -*/
    0x2518, /*line _|*/ 0x250c, /*line |~ */ /* 0x7c-0x7f */
};

ushort CCtableG3[0x60] =
{
//   0 1  2  3    4  5  6  7     8  9  a  b    c  d  e  f
    '#', /* [CC] closed captioning logo */
       0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xa0-0xaf */
    0, 0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xb0-0xbf */

    0, 0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xc0-0xcf */
    0, 0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xd0-0xdf */

    0, 0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xe0-0xff */
    0, 0, 0, 0,   0, 0, 0, 0,    0, 0, 0, 0,   0, 0, 0, 0, /* 0xf0-0xff */
};
