/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package com.aacsla.bluray.online;

import java.io.File;
import java.io.FileInputStream;

public class ContentAttribute {
    public ContentAttribute() {
    }

    public byte[] getContentCertID() {
        byte[] id = getContentCertID("AACS" + File.separator + "Content000.cer");
        if (id != null) {
            return id;
        }

        id = getContentCertID("MAKEMKV" + File.separator + "AACS" + File.separator + "Content000.cer");
        if (id != null) {
            return id;
        }

        id = getContentCertID("ANY!" + File.separator + "Content000.cer");
        if (id != null) {
            return id;
        }

        return new byte[6];
    }

    private byte[] getContentCertID(String file) {
        FileInputStream is = null;
        try {
            is = new FileInputStream(
                System.getProperty("bluray.vfs.root") + File.separator + file);
        } catch (Exception e) {
            return null;
        }
        try {
            if (is.skip(14) != 14)
                return null;
            byte[] bytes = new byte[6];
            if (is.read(bytes, 0, 6) != 6)
                return null;
            return bytes;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        } finally {
            if (is != null) {
                try {
                    is.close();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
    }
}
