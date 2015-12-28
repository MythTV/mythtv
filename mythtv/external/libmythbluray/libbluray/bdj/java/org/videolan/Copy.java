/*
 * This file is part of libbluray
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamClass;
import java.io.Serializable;

public class Copy {

    public static Serializable deepCopy(ClassLoader cl, Serializable obj) throws IOException, ClassNotFoundException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        ObjectOutputStream oos = new ObjectOutputStream(bos);
        try {
            oos.writeObject(obj);
        } catch (Exception e) {
            System.err.println("deepCopy write failed: " + e);
        } finally {
            oos.close();
        }

        ClObjectInputStream ios = new ClObjectInputStream(cl, new ByteArrayInputStream(bos.toByteArray()));
        Serializable s = (Serializable)ios.readObject();
        ios.close();
        return s;
    }

    /* ObjectInputStream with xlet class loader */
    private static class ClObjectInputStream extends ObjectInputStream {
        private ClassLoader classLoader = null;

        public ClObjectInputStream(ClassLoader cl, InputStream in) throws IOException {
            super(in);
            classLoader = cl;
        }

        protected Class resolveClass(ObjectStreamClass desc) throws IOException, ClassNotFoundException {
            try {
                return Class.forName(desc.getName(), false, classLoader);
            } catch (ClassNotFoundException e) {
                Class cl = super.resolveClass(desc);
                if (cl != null) {
                    return cl;
                }
                System.err.println("deepCopy: failed to resolve class " + desc.getName());
                throw e;
            } catch (Exception t) {
                System.err.println("deepCopy: failed to resolve class " + desc.getName() + ": " + t);
                return null;
            }
        }
    }
}
