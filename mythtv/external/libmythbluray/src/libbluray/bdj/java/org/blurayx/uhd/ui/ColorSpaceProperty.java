/*
 * This file is part of libbluray
 * Copyright (C) 2017  VideoLAN
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

package org.blurayx.uhd.ui;

public class ColorSpaceProperty {

    private String name = null;

    public static final ColorSpaceProperty BT709 = new ColorSpaceProperty("BT.709");
    public static final ColorSpaceProperty BT2020 = new ColorSpaceProperty("BT.2020");

    protected ColorSpaceProperty(String name) {
        this.name = name;
        if (name == null) {
            throw new NullPointerException("name is null");
        }
    }

    public String toString() {
        return getClass().getName() + "[" + name + "]";
    }
}
