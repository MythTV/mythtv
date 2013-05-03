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

package javax.media.protocol;

import java.lang.Character;

public class ContentDescriptor {
    public ContentDescriptor(String cdName)
    {
        this.cdName = cdName;
    }

    public String getContentType()
    {
        return cdName;
    }

    public static final String mimeTypeToPackageName(String mimeType)
    {
        char[] name = mimeType.toLowerCase().toCharArray();
        for (int i = 0; i < name.length; i++) {
            char c = name[i];
            if (c == '/')
                name[i] = '.';
            else if (!Character.isLetterOrDigit(c))
                name[i] = '_';
        }

        return new String(name);
    }

    public static final String CONTENT_UNKNOWN = "UnknownContent";

    private String cdName;
}
