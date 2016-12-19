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

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;

public class HKeyCapabilities {
    protected HKeyCapabilities() {
    }

    public static boolean getInputDeviceSupported() {
        return true;
    }

    public static boolean isSupported(int keycode) {
        return Arrays.binarySearch(supportedKeyCode, keycode) >= 0;
    }

    private static final int[] supportedKeyCode;

    static {
        ArrayList list = new ArrayList();
        Field[] fields = org.bluray.ui.event.HRcEvent.class.getFields();
        for (int i = 0; i < fields.length; i++) {
            String name = fields[i].getName();
            if ((name.startsWith("VK_")) && !(name.equals("VK_UNDEFINED"))) {
                try {
                    Integer keyCode = new Integer(fields[i].getInt(null));
                    list.add(keyCode);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        supportedKeyCode = new int[list.size()];
        for (int i = 0; i < list.size(); i++)
            supportedKeyCode[i] = ((Integer)list.get(i)).intValue();
        Arrays.sort(supportedKeyCode);
    }
}
