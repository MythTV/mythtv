// -*- Mode: c++ -*-

#include "teletextextractorreader.h"

void TeletextExtractorReader::PageUpdated(int page, int subpage)
{
    m_updatedPages.insert(qMakePair(page, subpage));
    TeletextReader::PageUpdated(page, subpage);
}

void TeletextExtractorReader::HeaderUpdated(
    int page, int subpage, tt_line_array& page_ptr, int lang)
{
    m_updatedPages.insert(qMakePair(page, subpage));
    TeletextReader::HeaderUpdated(page, subpage, page_ptr, lang);
}

/************************************************************************
 * Everything below this message in this file is based on some VLC
 * teletext code which was in turn based on some ProjectX teletext code.
 ************************************************************************/

/*****************************************************************************
 * telx.c : Minimalistic Teletext subtitles decoder
 *****************************************************************************
 * Copyright (C) 2007 Vincent Penne
 * Some code converted from ProjectX java dvb decoder (c) 2001-2005 by dvb.matt
 * $Id: 2b01e6a460b7c3693bccd690e3dbc018832d2777 $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*
 * My doc only mentions 13 national characters, but experiments show there
 * are more, in france for example I already found two more (0x9 and 0xb).
 *
 * Conversion is in this order :
 *
 * 0x23 0x24 0x40 0x5b 0x5c 0x5d 0x5e 0x5f 0x60 0x7b 0x7c 0x7d 0x7e
 * (these are the standard ones)
 * 0x08 0x09 0x0a 0x0b 0x0c 0x0d (apparently a control character) 0x0e 0x0f
 */

using ppi_natl_array = std::array<const uint16_t,21>;
static const std::array<const ppi_natl_array,13> ppi_national_subsets
{{
    { 0x00a3, 0x0024, 0x0040, 0x00ab, 0x00bd, 0x00bb, 0x005e, 0x0023,
      0x002d, 0x00bc, 0x00a6, 0x00be, 0x00f7 }, /* english, 000 */

    { 0x0023, 0x0024, 0x00a7, 0x00c4, 0x00d6, 0x00dc, 0x005e, 0x005f,
      0x00b0, 0x00e4, 0x00f6, 0x00fc, 0x00df }, /* german, 001 */

    { 0x0023, 0x00a4, 0x00c9, 0x00c4, 0x00d6, 0x00c5, 0x00dc, 0x005f,
      0x00e9, 0x00e4, 0x00f6, 0x00e5, 0x00fc
    }, /* swedish, finnish, hungarian, 010 */

    { 0x00a3, 0x0024, 0x00e9, 0x00b0, 0x00e7, 0x00bb, 0x005e, 0x0023,
      0x00f9, 0x00e0, 0x00f2, 0x00e8, 0x00ec }, /* italian, 011 */

    { 0x00e9, 0x00ef, 0x00e0, 0x00eb, 0x00ea, 0x00f9, 0x00ee, 0x0023,
      0x00e8, 0x00e2, 0x00f4, 0x00fb, 0x00e7, 0x0000, 0x00eb, 0x0000,
      0x00ef
    }, /* french, 100 */

    { 0x00e7, 0x0024, 0x00a1, 0x00e1, 0x00e9, 0x00ed, 0x00f3, 0x00fa,
      0x00bf, 0x00fc, 0x00f1, 0x00e8, 0x00e0 }, /* portuguese, spanish, 101 */

    { 0x0023, 0x016f, 0x010d, 0x0165, 0x017e, 0x00fd, 0x00ed, 0x0159,
      0x00e9, 0x00e1, 0x011b, 0x00fa, 0x0161 }, /* czech, slovak, 110 */

    { 0x0023, 0x00a4, 0x0162, 0x00c2, 0x015e, 0x0102, 0x00ce, 0x0131,
      0x0163, 0x00e2, 0x015f, 0x0103, 0x00ee }, /* rumanian, 111 */

    /* I have these tables too, but I don't know how they can be triggered */
    { 0x0023, 0x0024, 0x0160, 0x0117, 0x0119, 0x017d, 0x010d, 0x016b,
      0x0161, 0x0105, 0x0173, 0x017e, 0x012f }, /* lettish, lithuanian, 1000 */

    { 0x0023, 0x0144, 0x0105, 0x005a, 0x015a, 0x0141, 0x0107, 0x00f3,
      0x0119, 0x017c, 0x015b, 0x0142, 0x017a }, /* polish,  1001 */

    { 0x0023, 0x00cb, 0x010c, 0x0106, 0x017d, 0x0110, 0x0160, 0x00eb,
      0x010d, 0x0107, 0x017e, 0x0111, 0x0161
    }, /* serbian, croatian, slovenian, 1010 */

    { 0x0023, 0x00f5, 0x0160, 0x00c4, 0x00d6, 0x017e, 0x00dc, 0x00d5,
      0x0161, 0x00e4, 0x00f6, 0x017e, 0x00fc }, /* estonian, 1011 */

    { 0x0054, 0x011f, 0x0130, 0x015e, 0x00d6, 0x00c7, 0x00dc, 0x011e,
      0x0131, 0x015f, 0x00f6, 0x00e7, 0x00fc }, /* turkish, 1100 */
}};

// utc-2 --> utf-8
// this is not a general function, but it's enough for what we do here
// the result buffer need to be at least 4 bytes long
static void to_utf8(std::string &res, uint16_t ch)
{
    if(ch >= 0x80)
    {
        if(ch >= 0x800)
        {
            res = { static_cast<char>( (ch >> 12)        | 0xE0),
                    static_cast<char>(((ch >> 6) & 0x3F) | 0x80),
                    static_cast<char>( (ch & 0x3F)       | 0x80) };
        }
        else
        {
            res = { static_cast<char>((ch >> 6)   | 0xC0),
                    static_cast<char>((ch & 0x3F) | 0x80) } ;
        }
    }
    else
    {
        res = { static_cast<char>(ch) };
    }
}

/**
 * Get decoded ttx as a string.
 */

QString decode_teletext(int codePage, const tt_line_array& data)
{
    QString res;
    std::string utf8 {};

    const ppi_natl_array pi_active_national_set = ppi_national_subsets[codePage];

    for (int i = 0; i < 40; ++i)
    {
        //int in = bytereverse(data[i]) & 0x7f;
        int in = data[i] & 0x7f;
        uint16_t out = 32;

        switch (in)
        {
            /* special national characters */
            case 0x23:
                out = pi_active_national_set[0];
                break;
            case 0x24:
                out = pi_active_national_set[1];
                break;
            case 0x40:
                out = pi_active_national_set[2];
                break;
            case 0x5b:
                out = pi_active_national_set[3];
                break;
            case 0x5c:
                out = pi_active_national_set[4];
                break;
            case 0x5d:
                out = pi_active_national_set[5];
                break;
            case 0x5e:
                out = pi_active_national_set[6];
                break;
            case 0x5f:
                out = pi_active_national_set[7];
                break;
            case 0x60:
                out = pi_active_national_set[8];
                break;
            case 0x7b:
                out = pi_active_national_set[9];
                break;
            case 0x7c:
                out = pi_active_national_set[10];
                break;
            case 0x7d:
                out = pi_active_national_set[11];
                break;
            case 0x7e:
                out = pi_active_national_set[12];
                break;

            case 0x0a:
            case 0x0b:
            case 0x0d:
                //wtf? looks like some kind of garbage for me
                out = 32;
                break;

            default:
                /* non documented national range 0x08 - 0x0f */
                if (in >= 0x08 && in <= 0x0f)
                {
                    out = pi_active_national_set[13 + in - 8];
                    break;
                }

                /* normal ascii */
                if (in > 32 && in < 0x7f)
                    out = in;
        }

        /* handle undefined national characters */
        if (out == 0)
            out = '?'; //' ' or '?' ?

        /* convert to utf-8 */
        to_utf8(utf8, out);
        res += QString::fromUtf8(utf8.c_str());
    }

    return res;
}

//QString DechiperTtxFlags(int flags) {
//    QString res;

//    if (flags & TP_SUPPRESS_HEADER)
//        res += "TP_SUPPRESS_HEADER ";
//    if (flags & TP_UPDATE_INDICATOR)
//        res += "TP_UPDATE_INDICATOR ";
//    if (flags & TP_INTERRUPTED_SEQ)
//        res += "TP_INTERRUPTED_SEQ ";
//    if (flags & TP_INHIBIT_DISPLAY)
//        res += "TP_INHIBIT_DISPLAY ";
//    if (flags & TP_MAGAZINE_SERIAL)
//        res += "TP_MAGAZINE_SERIAL ";
//    if (flags & TP_ERASE_PAGE)
//        res += "TP_ERASE_PAGE ";
//    if (flags & TP_NEWSFLASH)
//        res += "TP_NEWSFLASH ";
//    if (flags & TP_SUBTITLE)
//        res += "TP_SUBTITLE ";

//    return res.trimmed();
//}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
