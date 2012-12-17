/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package org.videolan;

import java.awt.Dimension;

import org.bluray.ti.CodingType;

public class StreamInfo {
    public StreamInfo(byte coding_type, byte format, byte rate,
                      char char_code, String lang, byte aspect, byte subpath_id) {
        this.coding_type = coding_type;
        this.format = format;
        this.rate = rate;
        this.char_code = char_code;
        this.lang = lang;
        this.aspect = aspect;
        this.subpath_id = subpath_id;
    }

    public CodingType getCodingType() {
        switch (coding_type) {
        case (byte)0x02:
            return CodingType.MPEG2_VIDEO;
        case (byte)0x1b:
            return CodingType.MPEG4_AVC_VIDEO;
        case (byte)0xea:
            return CodingType.SMPTE_VC1_VIDEO;
        case (byte)0x80:
            return CodingType.LPCM_AUDIO;
        case (byte)0x81:
            return CodingType.DOLBY_AC3_AUDIO;
        case (byte)0x82:
            return CodingType.DTS_AUDIO;
        case (byte)0x83:
            return CodingType.DOLBY_LOSSLESS_AUDIO;
        case (byte)0x84:
        case (byte)0xA1:
            return CodingType.DOLBY_DIGITAL_PLUS_AUDIO;
        case (byte)0x85:
            return CodingType.DTS_HD_AUDIO_EXCEPT_XLL;
        case (byte)0x86:
            return CodingType.DTS_HD_AUDIO_XLL;
        case (byte)0xA2:
            return CodingType.DTS_HD_AUDIO_LBR;
        //FIXME:case (byte)0x??:
        //    return CodingType.DRA_AUDIO;
        //FIXME:case (byte)0x??:
        //    return CodingType.DRA_EXTENSION_AUDIO;
        case (byte)0x90:
            return CodingType.PRESENTATION_GRAPHICS;
        case (byte)0x91:
            return CodingType.INTERACTIVE_GRAPHICS;
        case (byte)0x92:
            return CodingType.TEXT_SUBTITLE;
        default:
            return null;
        }
    }

    public byte getFormat() {
        return format;
    }

    public Dimension getVideoSize() {
        int width, height;
        switch (format) {
        case (byte)0x01:
        case (byte)0x03:
            width = 720;
            height = 480;
            break;
        case (byte)0x02:
        case (byte)0x07:
            width = 720;
            height = 576;
            break;
        case (byte)0x05:
            width = 1280;
            height = 720;
            break;
        case (byte)0x04:
        case (byte)0x06:
            width = 1920;
            height = 1080;
            break;
        default:
                return null;
        }
        return new Dimension(width, height);
    }

    public Dimension getVideoAspectRatio() {
        int x, y;
        switch (aspect) {
        case (byte)0x02:
            x = 4;
            y = 3;
            break;
        case (byte)0x03:
            x = 16;
            y = 9;
            break;
        default:
                return null;
        }
        return new Dimension(x, y);
    }

    public byte getRate() {
        return rate;
    }

    public char getChar_code() {
        return char_code;
    }

    public String getLang() {
        return lang;
    }

    public int getSubPathId() {
        return subpath_id;
    }

    private byte coding_type;
    private byte format;
    private byte rate;
    private char char_code;
    private String lang;
    private byte aspect;
    private byte subpath_id;
}
