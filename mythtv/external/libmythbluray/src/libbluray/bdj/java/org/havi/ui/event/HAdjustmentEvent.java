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

import java.awt.AWTEvent;
import org.havi.ui.HAdjustmentInputPreferred;

public class HAdjustmentEvent extends AWTEvent {
    public HAdjustmentEvent(HAdjustmentInputPreferred source, int id)
    {
        super(source, id);
    }

    public static final int ADJUST_FIRST = RESERVED_ID_MAX + 1;
    public static final int ADJUST_LAST = ADJUST_FIRST + 5;
    public static final int ADJUST_START_CHANGE = ADJUST_FIRST;
    public static final int ADJUST_LESS = ADJUST_FIRST + 1;
    public static final int ADJUST_MORE = ADJUST_FIRST + 2;
    public static final int ADJUST_PAGE_LESS = ADJUST_FIRST + 3;
    public static final int ADJUST_PAGE_MORE = ADJUST_FIRST + 4;
    public static final int ADJUST_END_CHANGE = ADJUST_FIRST + 5;

    private static final long serialVersionUID = 1999888292949561391L;
}
