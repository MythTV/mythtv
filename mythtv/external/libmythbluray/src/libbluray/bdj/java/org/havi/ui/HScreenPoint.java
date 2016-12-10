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

public class HScreenPoint {
    public HScreenPoint(float x, float y) {
        setLocation(x, y);
    }

    public void setLocation(float x, float y) {
        this.x = x;
        this.y = y;
    }

    public boolean equals(Object obj)
    {
        if (!(obj instanceof HScreenPoint))
            return false;

        HScreenPoint other = (HScreenPoint)obj;
        Float x1 = new Float(this.x);
        Float y1 = new Float(this.y);
        Float x2 = new Float(other.x);
        Float y2 = new Float(other.y);
        return x1.equals(x2) && y1.equals(y2);
    }

    public String toString() {
        return "[" + x + "," + y + "]";
    }

    public float x;
    public float y;
}
