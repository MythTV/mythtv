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

import org.havi.ui.HKeyboardInputPreferred;
import java.awt.AWTEvent;

public class HTextEvent extends AWTEvent {
    public HTextEvent(HKeyboardInputPreferred source, int id)
    {
        super(source, id);
    }

    public static final int TEXT_FIRST = HItemEvent.ITEM_LAST + 1;
    public static final int TEXT_LAST = TEXT_FIRST + 9;
    public static final int TEXT_START_CHANGE = TEXT_FIRST;
    public static final int TEXT_CHANGE = TEXT_FIRST + 1;
    public static final int TEXT_CARET_CHANGE = TEXT_FIRST + 2;
    public static final int TEXT_END_CHANGE = TEXT_FIRST + 3;
    public static final int CARET_NEXT_CHAR = TEXT_FIRST + 4;
    public static final int CARET_NEXT_LINE = TEXT_FIRST + 5;
    public static final int CARET_PREV_CHAR = TEXT_FIRST + 6;
    public static final int CARET_PREV_LINE = TEXT_FIRST + 7;
    public static final int CARET_NEXT_PAGE = TEXT_FIRST + 8;
    public static final int CARET_PREV_PAGE = TEXT_FIRST + 9;

    private static final long serialVersionUID = -3765036613785847881L;
}
