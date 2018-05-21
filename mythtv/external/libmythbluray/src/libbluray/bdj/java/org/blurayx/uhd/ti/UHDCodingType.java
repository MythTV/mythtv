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

package org.blurayx.uhd.ti;

import java.lang.reflect.Constructor;
import java.security.AccessController;
import java.security.PrivilegedAction;

import org.bluray.ti.CodingType;

public class UHDCodingType {

    public static final CodingType HEVC_VIDEO =
        (CodingType)AccessController.doPrivileged(new PrivilegedAction() {
                public Object run() {
                    CodingType type = null;
                    try {
                        Class classCodingType = Class.forName("org.bluray.ti.CodingType");
                        Constructor constructor = classCodingType.getDeclaredConstructor(new Class[] { String.class });
                        constructor.setAccessible(true);
                        type = (CodingType)constructor.newInstance((Object [])new String[] { "HEVC_VIDEO" });
                    }
                    catch (Exception e) {
                        System.err.println("UHDCodingType.HEVC_VIDEO initialization failed");
                        e.printStackTrace();
                    }
                    return type;
                }
            });
}
