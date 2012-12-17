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

import java.awt.Color;
import java.awt.Graphics2D;

public abstract class DVBGraphics extends Graphics2D {
    protected DVBGraphics(Graphics2D gfx)
    {
        this.gfx = gfx;
    }

    public abstract int[] getAvailableCompositeRules();

    public DVBColor getBestColorMatch(Color c)
    {
        return new DVBColor(c);
    }

    public abstract Color getColor();

    public abstract DVBAlphaComposite getDVBComposite();

    public int getType()
    {
        return type;
    }

    public abstract void setColor(Color c);

    public abstract void setDVBComposite(DVBAlphaComposite comp)
            throws UnsupportedDrawingOperationException;

    public String toString()
    {
        return getClass().getName() + "[font=" + getFont() + ",color="
                + getColor() + "]";
    }
    
    protected int type = DVBBufferedImage.TYPE_BASE;
    protected Graphics2D gfx;
}
