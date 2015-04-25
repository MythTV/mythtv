/*
 * This file is part of libbluray
 * Copyright (C) 2014  libbluray
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

package java.awt;

class PolyEdge {

    private int x1;
    private int y1;
    //private int x2;
    private int y2;
    private float m;
    private float c;
    private boolean vertical;

    PolyEdge(int x1, int y1, int x2, int y2) {

        // sort lowest to highest
        if (y2 < y1) {
            int swap;
            swap = x1; x1 = x2; x2 = swap;
            swap = y1; y1 = y2; y2 = swap;
        }

        this.x1 = x1;
        this.y1 = y1;
        //this.x2 = x2;
        this.y2 = y2;

        if (x1 == x2) {
            vertical = true;
            m = 0;
        } else {
            m = (float)(y2 - y1) / (float)(x2 - x1);
            c = (-x1 * m) + y1;
            vertical = false;
        }
    }

    public boolean intersects(int y) {

        if (y <= y2 && y >= y1 && y1 != y2) {
            return true;
        }

        return false;
    }

    public int intersectionX(int y) {

        if (vertical) {
            return x1;
        }

        return (int)(((y - c) / m) + 0.5f);
    }
}
