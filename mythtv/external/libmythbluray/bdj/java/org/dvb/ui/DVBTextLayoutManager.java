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

package org.dvb.ui;

import java.awt.Insets;

import org.havi.ui.HVisible;

public class DVBTextLayoutManager implements org.havi.ui.HTextLayoutManager {

    public void render(String markedUpString, java.awt.Graphics g, HVisible v,
            java.awt.Insets insets)
    {
        throw new Error("Not implemented");
    }

    public DVBTextLayoutManager()
    {
        throw new Error("Not implemented");
    }

    public DVBTextLayoutManager(int horizontalAlign, int verticalAlign,
            int lineOrientation, int startCorner, boolean wrap, int linespace,
            int letterspace, int horizontalTabSpace)
    {
        throw new Error("Not implemented");
    }

    public void setHorizontalAlign(int horizontalAlign)
    {
        throw new Error("Not implemented");
    }

    public void setVerticalAlign(int verticalAlign)
    {
        throw new Error("Not implemented");
    }

    public void setLineOrientation(int lineOrientation)
    {
        throw new Error("Not implemented");
    }

    public void setStartCorner(int startCorner)
    {
        throw new Error("Not implemented");
    }

    public void setTextWrapping(boolean wrap)
    {
        throw new Error("Not implemented");
    }

    public void setLineSpace(int lineSpace)
    {
        throw new Error("Not implemented");
    }

    public void setLetterSpace(int letterSpace)
    {
        throw new Error("Not implemented");
    }

    public void setHorizontalTabSpacing(int horizontalTabSpace)
    {
        throw new Error("Not implemented");
    }

    public int getHorizontalAlign()
    {
        return HORIZONTAL_START_ALIGN;
    }

    public int getVerticalAlign()
    {
        throw new Error("Not implemented");
    }

    public int getLineOrientation()
    {
        throw new Error("Not implemented");
    }

    public int getStartCorner()
    {
        throw new Error("Not implemented");
    }

    public boolean getTextWrapping()
    {
        throw new Error("Not implemented");
    }

    public int getLineSpace()
    {
        throw new Error("Not implemented");
    }

    public int getLetterSpace()
    {
        throw new Error("Not implemented");
    }

    public int getHorizontalTabSpacing()
    {
        throw new Error("Not implemented");
    }

    public void setInsets(Insets insets)
    {
        throw new Error("Not implemented");
    }

    public Insets getInsets()
    {
        throw new Error("Not implemented");
    }

    public void addTextOverflowListener(TextOverflowListener listener)
    {
        throw new Error("Not implemented");
    }

    public void removeTextOverflowListener(TextOverflowListener listener)
    {
        throw new Error("Not implemented");
    }
    
    public static final int HORIZONTAL_START_ALIGN = 1;
    public static final int HORIZONTAL_END_ALIGN = 2;
    public static final int HORIZONTAL_CENTER = 3;

    public static final int VERTICAL_START_ALIGN = 4;
    public static final int VERTICAL_END_ALIGN = 5;
    public static final int VERTICAL_CENTER = 6;

    public static final int LINE_ORIENTATION_HORIZONTAL = 10;
    public static final int LINE_ORIENTATION_VERTICAL = 11;

    public static final int START_CORNER_UPPER_LEFT = 20;
    public static final int START_CORNER_UPPER_RIGHT = 21;
    public static final int START_CORNER_LOWER_LEFT = 22;
    public static final int START_CORNER_LOWER_RIGHT = 23;
}
