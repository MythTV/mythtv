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

public class FontIndexData {
    public FontIndexData() {

    }
    
    public String getName() {
        return name;
    }
    
    public String getFormat() {
        return format;
    }
    
    public String getFileName() {
        return filename;
    }
    
    public int getMinSize() {
        return minSize;
    }
    
    public int getMaxSize() {
        return maxSize;
    }
    
    public int getStyle() {
        return style;
    }

    public String toString() {
        return "FontIndexData [filename=" + filename + ", format=" + format
                + ", maxSize=" + maxSize + ", minSize=" + minSize + ", name="
                + name + ", style=" + style + "]";
    }

    protected String name;
    protected String format;
    protected String filename;
    protected int minSize;
    protected int maxSize;
    protected int style;
}
