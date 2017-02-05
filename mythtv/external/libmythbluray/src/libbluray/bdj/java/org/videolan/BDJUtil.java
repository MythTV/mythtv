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

package org.videolan;

import java.text.DecimalFormat;

public class BDJUtil {
    /**
     * Make a five digit zero padded string based on an integer
     * Ex. integer 1 -> string "00001"
     *
     * @param id
     * @return
     */
    public static String makeFiveDigitStr(int id)
    {
        if (id < 0 || id > 99999) {
            System.err.println("Invalid ID: " + id);
            throw new IllegalArgumentException("Invalid ID " + id);
        }
        String s = "" + id;
        while (s.length() < 5) {
            s = "0" + s;
        }
        return s;
        /*
        DecimalFormat fmt = new DecimalFormat();
        fmt.setMaximumIntegerDigits(5);
        fmt.setMinimumIntegerDigits(5);
        fmt.setGroupingUsed(false);

        return fmt.format(id);
        */
    }

    /**
     * Make a path based on the disc root to an absolute path based on the filesystem of the computer
     * Ex. /BDMV/JAR/00000.jar -> /bluray/disc/mount/point/BDMV/JAR/00000.jar
     * @param path
     * @return
     */
    public static String discRootToFilesystem(String path)
    {
        String vfsRoot = System.getProperty("bluray.vfs.root");
        if (vfsRoot == null) {
            System.err.println("discRootToFilesystem(): disc root not set !");
            return path;
        }
        return vfsRoot + path;
    }
}
