/*
 * This file is part of libbluray
 * Copyright (C) 2010  Libbluray
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

import java.util.ArrayList;

public class StrUtil {
    public static String Join(String[] strings, String separator) {
        String result = "";
        if (strings != null) {
            result = strings[0];
            for (int i = 1; i < strings.length; i++) {
                result = result + separator + strings[i];
            }
        }
        return result;
    }

    public static String[] split(String str, char delimiter) {
        ArrayList elements = new ArrayList();
        int i, j = 0, len = str.length();
        for (i = 0; i < len; i++)
            if (str.charAt(i) == delimiter) {
                elements.add(str.substring(j, i));
                j = i + 1;
            }
        if (j < i)
            elements.add(str.substring(j, i));
        return (String[])elements.toArray(new String[elements.size()]);
    }

    public static String[] split(String str, char delimiter, int num) {
        ArrayList elements = new ArrayList();
        int i, j = 0, n = 0, len = str.length();
        for (i = 0; (i < len) && (n < num); i++)
            if (str.charAt(i) == delimiter) {
                elements.add(str.substring(j, i));
                j = i + 1;
                n++;
            }
        if ((n < num) && (j < i)) {
            elements.add(str.substring(j, i));
            n++;
        }
        return (String[])elements.toArray(new String[elements.size()]);
    }
}
