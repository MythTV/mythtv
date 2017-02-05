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

class BDFileSystemImpl extends BDFileSystem {

    public BDFileSystemImpl(FileSystem fs) {
        super(fs);
    }

    /* different in ME */
    public boolean checkAccess(File f, int access) {
        return fs.checkAccess(f, access);
    }

    /* Not in ME */
    public boolean setPermission(File f, int access, boolean enable, boolean owneronly) {
        return fs.setPermission(f, access, enable, owneronly);
    }

    /* Not in ME */
    public long getSpace(File f, int t) {
        return fs.getSpace(f, t);
    }
}
