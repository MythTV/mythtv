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

    /* Different in SE */
    public boolean checkAccess(File f, boolean write) {
        return fs.checkAccess(f, write);
    }

    /* Not in SE */
    public boolean deleteOnExit(File f) {
        return fs.deleteOnExit(f);
    }
}
