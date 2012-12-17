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

import java.util.LinkedList;

public class HEventGroup {
    public HEventGroup() {
    }

    public void addKey(int keycode) {
        keys.add(new Integer(keycode));
    }

    public void removeKey(int keycode) {
        keys.remove(new Integer(keycode));
    }

    public void addAllNumericKeys() {
        addKey(HRcEvent.VK_0);
        addKey(HRcEvent.VK_1);
        addKey(HRcEvent.VK_2);
        addKey(HRcEvent.VK_3);
        addKey(HRcEvent.VK_4);
        addKey(HRcEvent.VK_5);
        addKey(HRcEvent.VK_6);
        addKey(HRcEvent.VK_7);
        addKey(HRcEvent.VK_8);
        addKey(HRcEvent.VK_9);
    }

    public void addAllColourKeys() {
        addKey(HRcEvent.VK_COLORED_KEY_0);
        addKey(HRcEvent.VK_COLORED_KEY_1);
        addKey(HRcEvent.VK_COLORED_KEY_2);
        addKey(HRcEvent.VK_COLORED_KEY_3);
    }

    public void addAllArrowKeys() {
        addKey(HRcEvent.VK_LEFT);
        addKey(HRcEvent.VK_RIGHT);
        addKey(HRcEvent.VK_UP);
        addKey(HRcEvent.VK_DOWN);
    }

    public void removeAllNumericKeys() {
        removeKey(HRcEvent.VK_0);
        removeKey(HRcEvent.VK_1);
        removeKey(HRcEvent.VK_2);
        removeKey(HRcEvent.VK_3);
        removeKey(HRcEvent.VK_4);
        removeKey(HRcEvent.VK_5);
        removeKey(HRcEvent.VK_6);
        removeKey(HRcEvent.VK_7);
        removeKey(HRcEvent.VK_8);
        removeKey(HRcEvent.VK_9);
    }

    public void removeAllColourKeys() {
        removeKey(HRcEvent.VK_COLORED_KEY_0);
        removeKey(HRcEvent.VK_COLORED_KEY_1);
        removeKey(HRcEvent.VK_COLORED_KEY_2);
        removeKey(HRcEvent.VK_COLORED_KEY_3);
    }

    public void removeAllArrowKeys() {
        removeKey(HRcEvent.VK_LEFT);
        removeKey(HRcEvent.VK_RIGHT);
        removeKey(HRcEvent.VK_UP);
        removeKey(HRcEvent.VK_DOWN);
    }

    public int[] getKeyEvents() {
        int[] arr = new int[keys.size()];

        for (int i = 0; i < keys.size(); i++)
            arr[i] = keys.get(i).intValue();

        return arr;
    }

    private LinkedList<Integer> keys;
}
