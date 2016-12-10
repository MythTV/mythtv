 /*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

class Area {
    public int x0;
    public int y0;
    public int x1;
    public int y1;

    public Area() {
        clear();
    }

    public Area(int width, int height) {
        this(0, 0, width - 1, height - 1);
    }

    public Area(int x0, int y0, int x1, int y1) {
        this.x0 = x0;
        this.y0 = y0;
        this.x1 = x1;
        this.y1 = y1;
    }

    private void clear() {
        x0 = Integer.MAX_VALUE;
        y0 = Integer.MAX_VALUE;
        x1 = -1;
        y1 = -1;
    }

    public synchronized void add(int newx, int newy) {
        x0 = Math.min(x0, newx);
        x1 = Math.max(x1, newx);
        y0 = Math.min(y0, newy);
        y1 = Math.max(y1, newy);
    }

    public synchronized void add(Rectangle r) {
        if ((r.x | r.width | r.y | r.height) >= 0) {
            x0 = Math.min(x0, r.x);
            x1 = Math.max(x1, r.x + r.width - 1);
            y0 = Math.min(y0, r.y);
            y1 = Math.max(y1, r.y + r.height - 1);
        }
    }

    public synchronized boolean isEmpty() {
        return (x1 < x0) || (y1 < y0);
    }

    private synchronized Area getBounds() {
        return new Area(x0, y0, x1, y1);
    }

    protected synchronized Area getBoundsAndClear() {
        Area a = getBounds();
        clear();
        return a;
    }

    public String toString() {
        return getClass().getName() + "[" + x0 + "," + y0 + "-" + x1 + "," + y1 + "]";
    }
}
