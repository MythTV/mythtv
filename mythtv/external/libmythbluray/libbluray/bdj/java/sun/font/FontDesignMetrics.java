/*
 * This file is part of libbluray
 * Copyright (C) 2015  libbluray
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

package sun.font;

import java.awt.FontMetrics;
import java.awt.Font;

/*
 * This class is used to "fix" Java 7 java.awt.Component.getFontMetrics()
 *
 * one disc calls directly (new java.awt.Component() { ... } ).getFontMetrics(font)
 *
 */

public class FontDesignMetrics extends FontMetrics {

    protected interface GetFontMetricsAccess {
        public abstract FontDesignMetrics getFontMetrics(Font font);
    }

    private static GetFontMetricsAccess access;

    protected static void setGetFontMetricsAccess(GetFontMetricsAccess a) {
        access = a;
    }

    /*
     *
     */

    public static FontDesignMetrics getMetrics(Font font) {
        return access.getFontMetrics(font);
     }

    protected FontDesignMetrics(Font font) {
        super(font);
    }
}
