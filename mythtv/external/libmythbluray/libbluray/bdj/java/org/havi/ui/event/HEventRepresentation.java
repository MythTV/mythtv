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

package org.havi.ui.event;

import java.awt.Color;
import java.awt.Image;

public class HEventRepresentation extends Object {
    protected HEventRepresentation(boolean supported, String text, Color color, Image symbol) {
        this.supported = supported;
        this.text = text;
        this.color = color;
        this.symbol = symbol;

        type = ER_TYPE_NOT_SUPPORTED;
        if (text != null)
            type |= ER_TYPE_STRING;
        if (color != null)
            type |= ER_TYPE_COLOR;
        if (symbol != null)
            type |= ER_TYPE_SYMBOL;
    }

    public boolean isSupported() {
        return supported;
    }

    protected void setType(int aType) {
        type = aType;
    }

    public int getType() {
        return type;
    }

    protected void setColor(Color aColor) {
        color = aColor;
        if (color != null)
            type |= ER_TYPE_COLOR;
        else
            type &= ~ER_TYPE_COLOR;
    }

    public Color getColor() {
        return color;
    }

    protected void setString(String aText) {
        text = aText;
        if (text != null)
            type |= ER_TYPE_STRING;
        else
            type &= ~ER_TYPE_STRING;
    }

    public String getString() {
        return text;
    }

    protected void setSymbol(Image aSymbol) {
        symbol = aSymbol;
        if (symbol != null)
            type |= ER_TYPE_SYMBOL;
        else
            type &= ~ER_TYPE_SYMBOL;
    }

    public Image getSymbol() {
        return symbol;
    }

    public static final int ER_TYPE_NOT_SUPPORTED = 0;
    public static final int ER_TYPE_STRING = 1;
    public static final int ER_TYPE_COLOR = 2;
    public static final int ER_TYPE_SYMBOL = 4;

    private boolean supported;
    private String text;
    private Color color;
    private Image symbol;
    private int type;
}
