/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

public class Arrays {
    public static int hashCode(Object[] array) {
        if (array == null)
            return 0;
        int hash = 1;
        for (int i = 0; i < array.length; i++)
            hash = 31 * hash + (array[i] == null ? 0 : array[i].hashCode());
        return hash;
    }

    public static String toString(int[] array) {
        if (array == null)
            return "null";
        StringBuffer buffer = new StringBuffer();
        buffer.append('[');
        int length = array.length;
        for (int j = 0; j < length; j++) {
                buffer.append(array[j]);
            if (j < (length - 1))
                buffer.append(", ");
        }
        buffer.append(']');
        return buffer.toString();
    }

    public static String toString(Object[] array) {
        if (array == null)
            return "null";
        StringBuffer buffer = new StringBuffer();
        buffer.append('[');
        int length = array.length;
        for (int j = 0; j < length; j++) {
                buffer.append(String.valueOf(array[j]));
            if (j < (length - 1))
                buffer.append(", ");
        }
        buffer.append(']');
        return buffer.toString();
    }
}
