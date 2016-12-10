/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

package org.havi.ui;

import java.awt.Image;
import java.awt.Point;

public class HImageMatte implements HMatte {
    private Image data = null;
    private Point offset = new Point(0, 0);

    public HImageMatte() {
    }

    public HImageMatte(Image data) {
        this.data = data;
    }

    public void setMatteData(Image data) {
        this.data = data;
    }

    public Image getMatteData() {
        return this.data;
    }

    public void setOffset(Point p) {
        this.offset = p;
    }

    public Point getOffset() {
        return this.offset;
    }
}
