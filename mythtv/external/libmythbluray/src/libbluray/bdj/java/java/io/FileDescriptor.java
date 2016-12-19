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

import java.util.Iterator;
import java.util.List;
import java.util.ArrayList;

public final class FileDescriptor {

    /* for files used by JVM */
    private int  fd;
    private long handle;

    /* for files in BD VFS */
    private long fp;

    /* for files inside .jar */
    protected InputStream slave;

    private int useCount;

    public FileDescriptor() {
	fd = -1;
        handle = -1;
        fp = 0;
        slave = null;
        useCount = 0;
    }

    private FileDescriptor(int fd) {
        this();
	this.fd = fd;
    }

    public static final FileDescriptor in  = new FileDescriptor(0);
    public static final FileDescriptor out = new FileDescriptor(1);
    public static final FileDescriptor err = new FileDescriptor(2);

    public boolean valid() {
	return (fd != -1) || (handle != -1) || (fp != 0) || (slave != null);
    }

    public native void sync() throws SyncFailedException;

    private static native void initIDs();

    static {
	initIDs();
    }

    int incrementAndGetUseCount() {
        synchronized (this) {
            useCount = useCount + 1;
            return useCount;
        }
    }

    int decrementAndGetUseCount() {
        synchronized (this) {
            useCount = useCount - 1;
            return useCount;
        }
    }

    /* Java 8 */

    private List parents = null;
    private boolean closed = false;

    synchronized void attach(Closeable c) {
        if (parents == null) {
            parents = new ArrayList();
        }
        parents.add(c);
    }

    synchronized void closeAll(Closeable releaser) throws IOException {
        if (!closed) {
            IOException ex = null;
            closed = true;

            for (Iterator it = parents.iterator(); it.hasNext(); ) {
                Closeable c = (Closeable)it.next();
                try {
                    c.close();
                } catch (IOException ioe) {
                    if (ex != null)
                        ex = ioe;
                }
            }

            releaser.close();
            if (ex != null) {
                throw ex;
            }
        }
    }
}
