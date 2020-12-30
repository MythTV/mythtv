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

    private static Logger logger = null;

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

        if (file.isAbsolute()) {
            String cachedName = BDJLoader.getCachedFile(name);
            if (cachedName != name) {
                synchronized (FileInputStream.class) {
                if (logger == null) {
                    logger = Logger.getLogger(FileInputStream.class.getName());
                }
                }
                logger.info("Using cached " + cachedName + " for " + name);
                name = cachedName;
            }
            open(name);
        } else {
            /* relative paths are problematic ... */
            /* Those should be mapped to xlet home directory, which is inside .jar file. */

            String home = BDJXletContext.getCurrentXletHome();
            if (home == null) {
                synchronized (FileInputStream.class) {
                if (logger == null) {
                    logger = Logger.getLogger(FileInputStream.class.getName());
                }
                }
                logger.error("no home found for " + name + " at " + Logger.dumpStack());
                throw new FileNotFoundException(name);
            }
            open(home + name);
        }
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
        fd = fdObj;
    }

    private native int  readBytes(byte b[], int off, int len) throws IOException;
    private native int  close0();
    /* OpenJDK 6, OpenJDK 7, PhoneME, ... */
    private native void open(String name) throws FileNotFoundException;

    public  native int  read() throws IOException;
    public  native long skip(long n) throws IOException;
    public  native int  available() throws IOException;

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

        return readBytes(b, off, len);
    }

    public void close() throws IOException {
        close0();
    }

    public final FileDescriptor getFD() throws IOException {
        if (fd == null) {
            throw new IOException();
        }
        return fd;
    }

    /* not in J2ME
    public FileChannel getChannel() {}
    */

    private static native void initIDs();

    static {
        initIDs();
    }

    protected void finalize() throws IOException {
        if (fd != null) {
            if (fd != FileDescriptor.in) {
                close();
            }
        }
    }
}
