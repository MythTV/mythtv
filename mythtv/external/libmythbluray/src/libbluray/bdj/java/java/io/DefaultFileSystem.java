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

package java.io;

class DefaultFileSystem {

    static FileSystem getNativeFileSystem() {
        Exception e1, e2, e3;
        try {
            return (FileSystem)Class.forName(System.getProperty("org.videolan.bdj_filesystem")).newInstance();
        } catch (Exception e) {
            e3 = e;
        }
        try {
            return (FileSystem)Class.forName("java.io.UnixFileSystem").newInstance();
        } catch (Exception e) {
            e1 = e;
        }
        try {
            return (FileSystem)Class.forName("java.io.WinNTFileSystem").newInstance();
        } catch (Exception e) {
            e2 = e;
        }

        /* No way to recover from here */
        System.err.println("Unsupported native filesystem !\n\t" + e1 + "\n\t" + e2 + "\n\t" + e3);
        //Runtime.getRuntime().halt(-1);
        throw new Error("No filesystem implementation found");
    }

    public static FileSystem getFileSystem() {
        FileSystem nativeFs = getNativeFileSystem();
        return new BDFileSystemImpl(nativeFs);
    }
}
