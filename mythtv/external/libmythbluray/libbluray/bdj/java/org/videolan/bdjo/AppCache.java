/*
 * This file is part of libbluray
 * Copyright (C) 2010  VideoLAN
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

package org.videolan.bdjo;

public class AppCache {
    public AppCache(int type, String refToName, String language)
    {
        this.type = type;
        this.refToName = refToName;
        this.language = language;
    }

    public int getType()
    {
        return type;
    }

    public String getRefToName()
    {
        return refToName;
    }

    public String getLanguage()
    {
        return language;
    }

    public String toString()
    {
        return "AppCache [language=" + language + ", refToName=" + refToName + ", type=" + type + "]";
    }

    public static final int JAR_FILE = 1;
    public static final int DIRECTORY = 2;

    private int type;
    private String refToName;
    private String language;
}
