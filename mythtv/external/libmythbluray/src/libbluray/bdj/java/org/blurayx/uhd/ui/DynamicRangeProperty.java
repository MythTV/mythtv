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

public class DynamicRangeProperty {

    private String name = null;

    public static final DynamicRangeProperty SDR = new DynamicRangeProperty("SDR");
    public static final DynamicRangeProperty BDMV_HDR = new DynamicRangeProperty("BDMV_HDR");
    public static final DynamicRangeProperty OPTION_A = new DynamicRangeProperty("OPTION_A");
    public static final DynamicRangeProperty OPTION_B = new DynamicRangeProperty("OPTION_B");
    public static final DynamicRangeProperty OPTION_C = new DynamicRangeProperty("OPTION_C");
    public static final DynamicRangeProperty OPTION_D = new DynamicRangeProperty("OPTION_D");
    public static final DynamicRangeProperty OPTION_E = new DynamicRangeProperty("OPTION_E");
    public static final DynamicRangeProperty OPTION_F = new DynamicRangeProperty("OPTION_F");
    public static final DynamicRangeProperty OPTION_G = new DynamicRangeProperty("OPTION_G");
    public static final DynamicRangeProperty OPTION_H = new DynamicRangeProperty("OPTION_H");
    public static final DynamicRangeProperty OPTION_I = new DynamicRangeProperty("OPTION_I");
    public static final DynamicRangeProperty OPTION_J = new DynamicRangeProperty("OPTION_J");
    public static final DynamicRangeProperty OPTION_K = new DynamicRangeProperty("OPTION_K");
    public static final DynamicRangeProperty OPTION_L = new DynamicRangeProperty("OPTION_L");
    public static final DynamicRangeProperty OPTION_M = new DynamicRangeProperty("OPTION_M");
    public static final DynamicRangeProperty OPTION_N = new DynamicRangeProperty("OPTION_N");
    public static final DynamicRangeProperty OPTION_O = new DynamicRangeProperty("OPTION_O");
    public static final DynamicRangeProperty OPTION_P = new DynamicRangeProperty("OPTION_P");
    public static final DynamicRangeProperty OPTION_Q = new DynamicRangeProperty("OPTION_Q");
    public static final DynamicRangeProperty OPTION_R = new DynamicRangeProperty("OPTION_R");
    public static final DynamicRangeProperty OPTION_S = new DynamicRangeProperty("OPTION_S");
    public static final DynamicRangeProperty OPTION_T = new DynamicRangeProperty("OPTION_T");
    public static final DynamicRangeProperty OPTION_U = new DynamicRangeProperty("OPTION_U");
    public static final DynamicRangeProperty OPTION_V = new DynamicRangeProperty("OPTION_V");
    public static final DynamicRangeProperty OPTION_W = new DynamicRangeProperty("OPTION_W");
    public static final DynamicRangeProperty OPTION_X = new DynamicRangeProperty("OPTION_X");
    public static final DynamicRangeProperty OPTION_Y = new DynamicRangeProperty("OPTION_Y");
    public static final DynamicRangeProperty OPTION_Z = new DynamicRangeProperty("OPTION_Z");

    protected DynamicRangeProperty(String name) {
        this.name = name;
        if (name == null) {
            throw new NullPointerException("name is null");
        }
    }

    public String toString() {
        return getClass().getName() + "[" + name + "]";
    }
}
