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

package org.havi.ui;

public class HScreenRectangle {
    public HScreenRectangle(float x, float y, float width, float height) {
        setLocation(x, y);
        setSize(width, height);
    }

    public void setLocation(float x, float y) {
        this.x = x;
        this.y = y;
    }

    public void setSize(float width, float height) {
        this.width = Math.max(0.0f, width);
        this.height = Math.max(0.0f, height);
    }

    public boolean equals(Object obj)
    {
        if (!(obj instanceof HScreenRectangle))
            return false;

        HScreenRectangle other = (HScreenRectangle)obj;
        Float x1 = new Float(this.x);
        Float y1 = new Float(this.y);
        Float w1 = new Float(this.width);
        Float h1 = new Float(this.height);
        Float x2 = new Float(other.x);
        Float y2 = new Float(other.y);
        Float w2 = new Float(other.width);
        Float h2 = new Float(other.height);
        return x1.equals(x2) && y1.equals(y2) && w1.equals(w2) && h1.equals(h2);
    }

    public String toString() {
        return getClass().getName() + "[" + x + "," + y + " " + width + "x" + height + "]";
    }

    public float x;
    public float y;
    public float width;
    public float height;
}
