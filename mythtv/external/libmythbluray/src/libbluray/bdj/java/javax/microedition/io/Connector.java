/*
 * This file is part of libbluray
 * Copyright (C) 2019  VideoLAN
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

package javax.microedition.io;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.videolan.io.ConnectorImpl;

public class Connector {

    public static final int READ = 1;
    public static final int WRITE = 2;
    public static final int READ_WRITE = 3;

     public static Connection open(String name) throws IOException {
         return open(name, READ_WRITE);
     }

    public static Connection open(String name, int mode) throws IOException {
        return open(name, mode, false);
    }

    public static Connection open(String name, int mode, boolean timeouts) throws IOException {
        if (mode != READ && mode != WRITE && mode != READ_WRITE) {
            throw new IllegalArgumentException("invalid mode");
        }
        if (name == null) {
            throw new IllegalArgumentException("null URL");
        }
        if (name.equals("")) {
            throw new IllegalArgumentException("empty URL");
        }

        return ConnectorImpl.open(name, mode, timeouts);
    }

    public static DataInputStream openDataInputStream(String name) throws IOException {
        InputConnection ic = null;
        try {
            ic = (InputConnection)open(name, READ);
        } catch (ClassCastException cce) {
            throw new IllegalArgumentException(name);
        }

        try {
            return ic.openDataInputStream();
        } finally {
            ic.close();
        }
    }

    public static DataOutputStream openDataOutputStream(String name) throws IOException {
        OutputConnection oc = null;
        try {
            oc = (OutputConnection)open(name, WRITE);
        } catch (ClassCastException cce) {
            throw new IllegalArgumentException(name);
        }

        try {
            return oc.openDataOutputStream();
        } finally {
            oc.close();
        }
    }

    public static InputStream openInputStream(String name) throws IOException {
        return openDataInputStream(name);
    }

    public static OutputStream openOutputStream(String name)throws IOException {
        return openDataOutputStream(name);
    }
}
