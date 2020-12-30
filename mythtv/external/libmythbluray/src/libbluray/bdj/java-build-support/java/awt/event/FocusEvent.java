/*
 * This file is part of libbluray
 * Copyright (C) 2019  libbluray
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

package java.awt.event;

import java.awt.Component;

public class FocusEvent extends ComponentEvent {

    /*
     * Dummy class used during compilation
     *
     * This class is used at compile time to hide Java 8 / Java 9 differences in
     * java.awt.peer.*, java.awt.FocusEvent and sun.awt.CausedFocusEvent.
     *
     * This allows compiling special version of BDFramePeer
     * that will work both in Java < 9 and Java > 9.
     * Correct methods and dependencies are automatically selected at run time,
     * thanks to Java on-demand linking.
     *
     * NOTE:
     * This class is not complete and should not be included at runtime.
     *
     */

    static {
        if (System.getProperty("does_not_exist") == null)
            throw new Error("This class should not be included at run time");
    }

    public static class /* enum */ Cause {
    }

    /* Needed when compiling for HaVi */
    public static final int FOCUS_FIRST  = 1004;
    public static final int FOCUS_LAST   = 1005;
    public static final int FOCUS_GAINED = FOCUS_FIRST;
    public static final int FOCUS_LOST   = 1 + FOCUS_FIRST;

    public FocusEvent(Component src, int id, boolean t) {
        super(src, id);
    }

    public FocusEvent(Component src, int id) {
        super(src, id);
    }
}
