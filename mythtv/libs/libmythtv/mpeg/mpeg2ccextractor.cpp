/*
 * Modified from FFmpeg's mpeg_decode_a53_cc() from libavcodec/mpeg12dec.c
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2002-2013 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2026 Scott Theisen
 *
 * This file is part of MythTV.
 *
 * MythTV is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MythTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with MythTV; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mpeg2ccextractor.h"

#include <array>

#include <QString>

#include "libmythbase/mythlogging.h"

#include "bitreader.h"

static constexpr std::array<uint8_t, 256> k_reverse8 {
0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF,
};

void Mpeg2ClosedCaptionsExtractor::setFormatPrivate(Format format)
{
    if (m_format == Format::kAuto)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, QString("First seen closed captions are in the %1 format.")
            .arg(formatToString(format))
            );
    }
    m_format = format;
}

bool Mpeg2ClosedCaptionsExtractor::append(const uint8_t* buf, int buf_size, bool top_field_first)
{
    if ((m_format == Format::kAuto || m_format == Format::kA53_Part4) &&
        buf_size >= 6 &&
        buf[0] == 'G' && buf[1] == 'A' && buf[2] == '9' && buf[3] == '4' &&
        buf[4] == 3 /* user_data_type_code */ &&
        (buf[5] & 0x40) != 0 /* process_cc_data_flag */
        )
    {
        /* extract A/53 Part 4 CC data */
        LOG(VB_PLAYBACK, LOG_ALERT, QString("buf = %1 m_captions.size() = %2").arg(pointerToQString(buf)).arg(m_captions.size()));
        int cc_count = buf[5] & 0x1F;
        int length = cc_count * 3;
        buf_size -= 7;
        buf += 7;
        if (cc_count > 0 && buf_size >= length)
        {
            m_captions.insert(m_captions.end(), buf, buf + length);
            setFormatPrivate(Format::kA53_Part4);
        }
        return true;
    }
    if ((m_format == Format::kAuto || m_format == Format::kSCTE20) &&
        buf_size >= 2 &&
        buf[0] == 0x03 /* user_data_type_code */ &&
        (buf[1] & 0x7F) == 0x01 /* zero valued bits and vbi_data_flag */
        )
    {
        /* extract SCTE-20 CC data */
        BitReader scte20 {buf + 2, static_cast<uint64_t>(buf_size - 2)};
        int cc_count = scte20.get_bits(5);
        if (cc_count > 0)
        {
            m_captions.reserve(m_captions.size() + cc_count * 3);
            while (cc_count > 0 && scte20.get_bits_left() >= 26)
            {
                cc_count--;
                scte20.skip_bits(2); // cc_priority
                uint8_t field_number = scte20.get_bits(2);
                scte20.skip_bits(5); // line_offset
                uint8_t cc_data_1 = scte20.get_bits(8);
                uint8_t cc_data_2 = scte20.get_bits(8);
                scte20.skip_bits(1); // marker_bit

                switch (field_number)
                {
                default:
                case 0b00: // forbidden
                    m_captions.insert(m_captions.end(), 3, 0x00); // cc_valid is false
                    break;
                case 0b01: // 1st display field
                case 0b10: // 2nd display field
                case 0b11: // 3rd display field (the repeated field in film mode).
                    // treat field_number 3 as 1 and convert to CTA-708 cc_type
                    // CTA-708 cc_type is 0 for field 1 and 1 for field 2
                    field_number = (field_number - 1) & 0b1;
                    /* ANSI/SCTE 20 2017 (R2022) § 5.8.1 field_number:
                    Please note that this standard associates top field with field 1 or
                    odd field and bottom field with field 2 or even field.
                    */
                    if (!top_field_first)
                    {
                        field_number ^= 0b1;
                    }
                    m_captions.emplace_back(0xF8 | 0x04 /* valid */ | field_number);
                    m_captions.emplace_back(k_reverse8[cc_data_1]);
                    m_captions.emplace_back(k_reverse8[cc_data_2]);
                    break;
                }
            }

            setFormatPrivate(Format::kSCTE20);
        }
        return true;
    }
    if ((m_format == Format::kAuto || m_format == Format::kDVD) &&
        buf_size >= 11 &&
        buf[0] == 'C' && buf[1] == 'C' && buf[2] == 0x01 && buf[3] == 0xF8
        )
    {
        /* extract DVD CC data
         *
         * uint32_t   user_data_start_code        0x000001B2    (big endian)
         * uint16_t   user_identifier             0x4343 "CC"
         * uint8_t    user_data_type_code         0x01
         * uint8_t    caption_block_size          0xF8
         * uint8_t
         *   bit 7    caption_odd_field_first     1=odd field (CC1/CC2) first  0=even field (CC3/CC4) first
         *   bit 6    caption_filler              0
         *   bit 5:1  caption_block_count         number of caption blocks (pairs of caption words = frames). Most DVDs use 15 per start of GOP.
         *   bit 0    caption_extra_field_added   1=one additional caption word
         *
         * struct caption_field_block {
         *   uint8_t
         *     bit 7:1 caption_filler             0x7F (all 1s)
         *     bit 0   caption_field_odd          1=odd field (this is CC1/CC2)  0=even field (this is CC3/CC4)
         *   uint8_t   caption_first_byte
         *   uint8_t   caption_second_byte
         * } caption_block[(caption_block_count * 2) + caption_extra_field_added];
         *
         * Some DVDs encode caption data for both fields with caption_field_odd=1. The only way to decode the fields
         * correctly is to start on the field indicated by caption_odd_field_first and count between odd/even fields.
         * Don't assume that the first caption word is the odd field. There do exist MPEG files in the wild that start
         * on the even field. There also exist DVDs in the wild that encode an odd field count and the
         * caption_extra_field_added/caption_odd_field_first bits change per packet to allow that. */
        int cc_count = 0;
        // There is a caption count field in the data, but it is often
        // incorrect.  So count the number of captions present.
        for (int i = 5; i + 6 <= buf_size && ((buf[i] & 0xFE) == 0xFE); i += 6)
            cc_count++;
        // Transform the DVD format into A/53 Part 4 format
        if (cc_count > 0)
        {
            m_captions.reserve(m_captions.size() + cc_count * 6);
            bool field1 = (buf[4] & 0x80) != 0;
            buf += 5;
            while (cc_count > 0)
            {
                m_captions.emplace_back((buf[0] == 0xFF &&  field1) ? 0xFC : 0xFD);
                m_captions.emplace_back(buf[1]);
                m_captions.emplace_back(buf[2]);
                m_captions.emplace_back((buf[3] == 0xFF && !field1) ? 0xFC : 0xFD);
                m_captions.emplace_back(buf[4]);
                m_captions.emplace_back(buf[5]);
                buf += 6;
                cc_count--;
            }

            setFormatPrivate(Format::kDVD);
        }
        return true;
    }
    if ((m_format == Format::kAuto || m_format == Format::kDish) &&
        buf_size >= 12 &&
        buf[0] == 0x05 && buf[1] == 0x02
        )
    {
        /* extract Dish Network CC data */
        const uint8_t cc_header = 0xf8 | 0x04 /* valid */ | 0x00 /* line 21 field 1 */;
        uint8_t cc_data[4] = {0};
        uint8_t cc_count = 0;
        uint8_t cc_type = buf[7];
        buf_size -= 8;
        buf += 8;

        if (cc_type == 0x05 && buf_size >= 7)
        {
            cc_type = buf[6];
            buf_size -= 7;
            buf += 7;
        }

        if (cc_type == 0x02 && buf_size >= 4)
        {
            /* 2-byte caption, can be repeated */
            cc_count = 1;
            cc_data[0] = buf[1];
            cc_data[1] = buf[2];
            cc_type = buf[3];

            /* Only repeat characters when the next type flag
             * is 0x04 and the characters are repeatable (i.e., less than
             * 32 with the parity stripped).
             */
            if (cc_type == 0x04 && (cc_data[0] & 0x7f) < 32) {
                cc_count = 2;
                cc_data[2] = cc_data[0];
                cc_data[3] = cc_data[1];
            }
        }
        else if (cc_type == 0x04 && buf_size >= 5)
        {
            /* 4-byte caption, not repeated */
            cc_count = 2;
            cc_data[0] = buf[1];
            cc_data[1] = buf[2];
            cc_data[2] = buf[3];
            cc_data[3] = buf[4];
        }

        if (cc_count > 0)
        {
            m_captions.reserve(m_captions.size() + cc_count * 3);
            m_captions.emplace_back(cc_header);
            m_captions.emplace_back(cc_data[0]);
            m_captions.emplace_back(cc_data[1]);
            if (cc_count == 2)
            {
                m_captions.emplace_back(cc_header);
                m_captions.emplace_back(cc_data[2]);
                m_captions.emplace_back(cc_data[3]);
            }

            setFormatPrivate(Format::kDish);
        }
        return true;
    }
    return false;
}
