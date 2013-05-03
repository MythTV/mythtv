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

public class StreamType extends javax.tv.service.navigation.StreamType {
    public StreamType(String name)
    {
        super(name);
        this.name = name;
    }

    public String toString()
    {
        return name;
    }

    public static final StreamType DOLBY_AC3L_AUDIO = new StreamType(
            "DOLBY_AC3L_AUDIO");
    public static final StreamType DTS_AUDIO = new StreamType("DTS_AUDIO");
    public static final StreamType LPCM_AUDIO = new StreamType("LPCM_AUDIO");
    public static final StreamType MPEG2_VIDEO = new StreamType("MPEG2_VIDEO");
    public static final StreamType MPEG4_AVC_VIDEO = new StreamType(
            "MPEG4_AVC_VIDEO");
    public static final StreamType PRESENTATION_GRAPHIC = new StreamType(
            "PRESENTATION_GRAPHIC");
    public static final StreamType SMPTE_VIDEO = new StreamType("SMPTE_VIDEO");
    public static final StreamType TEXT_SUBTITLE = new StreamType(
            "TEXT_SUBTITLE");

    private String name;
}
