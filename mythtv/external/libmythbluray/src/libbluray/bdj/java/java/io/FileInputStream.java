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


import org.videolan.BDJLoader;
import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class FileInputStream extends InputStream
{
    private FileDescriptor fd;

    private final Object closeLock = new Object();
    private volatile boolean closed = false;

    private static Logger logger = null;

    private int available = 0;

    public FileInputStream(File file) throws FileNotFoundException {
        String name = file != null ? file.getPath() : null;

        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            security.checkRead(name);
        }
        if (name == null) {
            throw new NullPointerException();
        }

        fd = new FileDescriptor();
        fd.incrementAndGetUseCount();

        if (file.isAbsolute()) {
            String cachedName = BDJLoader.getCachedFile(name);
            if (cachedName != name) {
                if (logger == null) {
                    logger = Logger.getLogger(FileInputStream.class.getName());
                }
                logger.info("Using cached " + cachedName + " for " + name);
                name = cachedName;
            }
            openImpl(name);
        } else {
            /* relative paths are problematic ... */
            /* Those should be mapped to xlet home directory, which is inside .jar file. */

            String home = BDJXletContext.getCurrentXletHome();
            if (home == null) {
                if (logger == null) {
                    logger = Logger.getLogger(FileInputStream.class.getName());
                }
                logger.error("no home found for " + name + " at " + Logger.dumpStack());
                throw new FileNotFoundException(name);
            }
            openImpl(home + name);
        }

        available = 1024;
    }

    public FileInputStream(String name) throws FileNotFoundException {
        this(name != null ? new File(name) : null);
    }

    public FileInputStream(FileDescriptor fdObj) {
        SecurityManager security = System.getSecurityManager();
        if (fdObj == null) {
            throw new NullPointerException();
        }
        if (security != null) {
            security.checkRead(fdObj);
        }
        fdObj.incrementAndGetUseCount();
        fd = fdObj;
        available = 1024;
    }

    /* open()/open0() wrapper to select correct native method at runtime */
    private void openImpl(String name) throws FileNotFoundException {
        try {
            open(name);
        } catch (UnsatisfiedLinkError e) {
            /* OpenJDK 8 b40 */
            open0(name);
        }
    }

    private native int  readBytes(byte b[], int off, int len) throws IOException;
    private native int  close0();
    /* OpenJDK 6, OpenJDK 7, PhoneME, ... */
    private native void open(String name) throws FileNotFoundException;
    /* OpenJDK 8 */
    private native void open0(String name) throws FileNotFoundException;

    //public  native int  read() throws IOException;
    //public  native long skip(long n) throws IOException;
    //public  native int  available() throws IOException;

    public int available() throws IOException {
        if (fd != null && fd.slave != null) {
            return fd.slave.available();
        }
        return available;
    }

    public  int  read() throws IOException {
        byte b[] = new byte[1];
        if (read(b) == 1)
            return b[0];
        return -1;
    }

    public int read(byte b[]) throws IOException {
        return read(b, 0, b.length);
    }

    public int read(byte b[], int off, int len) throws IOException {
        if (b == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off > b.length || (off + len) > b.length || (off + len) < 0) {
            throw new IndexOutOfBoundsException();
        }

        if (fd != null && fd.slave != null) {
            return fd.slave.read(b, off, len);
        }

        int r = readBytes(b, off, len);
        if (r != len) {
            available = 0;
        }
        return r;
    }

    public long skip(long n) throws IOException {
        return super.skip(n);
    }

    public void close() throws IOException {
        close(true);
    }

    public void close(boolean force) throws IOException {
        synchronized (closeLock) {
            if (closed) {
                return;
            }
            closed = true;
        }

        available = 0;

        if (fd != null) {
            if (fd.slave != null) {
                fd.slave.close();
                return;
            }

            int n = fd.decrementAndGetUseCount();
            if (n > 0 && !force) {
                return;
            }
        }

        close0();
    }

    public final FileDescriptor getFD() throws IOException {
        if (fd == null) {
            throw new IOException();
        }
        return fd;
    }

    /* not in J2SE
    public FileChannel getChannel() {}
    */

    private static native void initIDs();

    static {
        initIDs();
    }

    protected void finalize() throws IOException {
        if (fd != null) {
            if (fd != FileDescriptor.in) {
                close(false);
            }
        }
    }
}
