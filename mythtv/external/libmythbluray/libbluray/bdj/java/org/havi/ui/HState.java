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

public interface HState {
    public static final int FOCUSED_STATE_BIT = 0x01;
    public static final int ACTIONED_STATE_BIT = 0x02;
    public static final int DISABLED_STATE_BIT = 0x04;

    public static final int FIRST_STATE = 0x80;
    public static final int NORMAL_STATE = 0x80;
    public static final int FOCUSED_STATE = 0x81;
    public static final int ACTIONED_STATE = 0x82;
    public static final int ACTIONED_FOCUSED_STATE = 0x83;
    public static final int DISABLED_STATE = 0x84;
    public static final int DISABLED_FOCUSED_STATE = 0x85;
    public static final int DISABLED_ACTIONED_STATE = 0x86;
    public static final int DISABLED_ACTIONED_FOCUSED_STATE = 0x87;
    public static final int ALL_STATES = 0x07;
    public static final int LAST_STATE = 0x87;
}
