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

import java.awt.Point;

public interface HAdjustableLook extends HLook {
    public int hitTest(HOrientable component, Point point);

    public Integer getValue(HOrientable component, Point point);

    public static final int ADJUST_NONE = -1;
    public static final int ADJUST_BUTTON_LESS = -2;
    public static final int ADJUST_BUTTON_MORE = -3;
    public static final int ADJUST_PAGE_LESS = -4;
    public static final int ADJUST_PAGE_MORE = -5;
    public static final int ADJUST_THUMB = -6;
}
