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

public class TerminalInfo {
    public TerminalInfo(String defaultFont, int resolution, boolean menuCallMask, boolean titleSearchMask)
    {
        this.defaultFont = defaultFont;
        this.resolution = GraphicsResolution.fromId(resolution);
        this.menuCallMask = menuCallMask;
        this.titleSearchMask = titleSearchMask;
    }

    public String getDefaultFont()
    {
        return defaultFont;
    }

    public GraphicsResolution getResolution()
    {
        return resolution;
    }

    public boolean getMenuCallMask()
    {
        return menuCallMask;
    }

    public boolean getTitleSearchMask()
    {
        return titleSearchMask;
    }

    public String toString()
    {
        return "TerminalInfo [defaultFont=" + defaultFont + ", menuCallMask="
                + menuCallMask + ", resolution=" + resolution
                + ", titleSearchMask=" + titleSearchMask + "]";
    }

    private String defaultFont;
    private GraphicsResolution resolution;
    private boolean menuCallMask;
    private boolean titleSearchMask;
}
