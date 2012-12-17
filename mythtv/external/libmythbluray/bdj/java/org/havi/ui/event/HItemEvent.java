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

import org.havi.ui.HSelectionInputPreferred;

public class HItemEvent extends AWTEvent {
    public HItemEvent(HSelectionInputPreferred source, int id, Object item)
    {
        super(source, id);

        this.item = item;
    }

    public Object getItem()
    {
        return item;
    }

    public static final int ITEM_FIRST = HAdjustmentEvent.ADJUST_LAST + 1;
    public static final int ITEM_START_CHANGE = ITEM_FIRST;
    public static final int ITEM_TOGGLE_SELECTED = ITEM_FIRST + 1;
    public static final int ITEM_SELECTED = ITEM_FIRST + 2;
    public static final int ITEM_CLEARED = ITEM_FIRST + 3;
    public static final int ITEM_SELECTION_CLEARED = ITEM_FIRST + 4;
    public static final int ITEM_SET_CURRENT = ITEM_FIRST + 5;
    public static final int ITEM_SET_PREVIOUS = ITEM_FIRST + 6;
    public static final int ITEM_SET_NEXT = ITEM_FIRST + 7;
    public static final int SCROLL_MORE = ITEM_FIRST + 8;
    public static final int SCROLL_LESS = ITEM_FIRST + 9;
    public static final int SCROLL_PAGE_MORE = ITEM_FIRST + 10;
    public static final int SCROLL_PAGE_LESS = ITEM_FIRST + 11;
    public static final int ITEM_END_CHANGE = ITEM_FIRST + 12;
    public static final int ITEM_LAST = ITEM_FIRST + 12;

    private Object item;
    private static final long serialVersionUID = -487680187698958380L;
}
