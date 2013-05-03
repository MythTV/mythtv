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

package org.bluray.ti;

public class CodingType {
    protected CodingType(String name)
    {
        this.name = name;
    }

    public String toString()
    {
        return name;
    }

    public static final CodingType DOLBY_AC3_AUDIO = new CodingType(
            "DOLBY_AC3_AUDIO");
    public static final CodingType DOLBY_DIGITAL_PLUS_AUDIO = new CodingType(
            "DOLBY_DIGITAL_PLUS_AUDIO");
    public static final CodingType DOLBY_LOSSLESS_AUDIO = new CodingType(
            "DOLBY_LOSSLESS_AUDIO");

    public static final CodingType DRA_AUDIO = new CodingType("DRA_AUDIO");
    public static final CodingType DRA_EXTENSION_AUDIO = new CodingType(
            "DRA_EXTENSION_AUDIO");

    public static final CodingType DTS_AUDIO = new CodingType("DTS_AUDIO");
    public static final CodingType DTS_HD_AUDIO = new CodingType("DTS_HD_AUDIO");
    public static final CodingType DTS_HD_AUDIO_EXCEPT_XLL = new CodingType(
            "DTS_HD_AUDIO_EXCEPT_XLL");
    public static final CodingType DTS_HD_AUDIO_LBR = new CodingType(
            "DTS_HD_AUDIO_LBR");
    public static final CodingType DTS_HD_AUDIO_XLL = new CodingType(
            "DTS_HD_AUDIO_XLL");

    public static final CodingType INTERACTIVE_GRAPHICS = new CodingType(
            "INTERACTIVE_GRAPHICS");
    public static final CodingType LPCM_AUDIO = new CodingType("LPCM_AUDIO");
    public static final CodingType MPEG2_VIDEO = new CodingType("MPEG2_VIDEO");
    public static final CodingType MPEG4_AVC_VIDEO = new CodingType(
            "MPEG4_AVC_VIDEO");
    public static final CodingType PRESENTATION_GRAPHICS = new CodingType(
            "PRESENTATION_GRAPHICS");
    public static final CodingType SMPTE_VC1_VIDEO = new CodingType(
            "SMPTE_VC1_VIDEO");
    public static final CodingType TEXT_SUBTITLE = new CodingType(
            "TEXT_SUBTITLE");

    private String name;
}
