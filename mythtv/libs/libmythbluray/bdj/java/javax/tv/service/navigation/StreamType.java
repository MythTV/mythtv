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

package javax.tv.service.navigation;

public class StreamType {
    protected StreamType(String name)
    {
        this.name = name;
    }

    public String toString()
    {
        return name;
    }

    public static final StreamType VIDEO = new StreamType("VIDEO");
    public static final StreamType AUDIO = new StreamType("AUDIO");
    public static final StreamType SUBTITLES = new StreamType("SUBTITLES");
    public static final StreamType DATA = new StreamType("DATA");
    public static final StreamType SECTIONS = new StreamType("SECTIONS");
    public static final StreamType UNKNOWN = new StreamType("UNKNOWN");
    private String name;
}
