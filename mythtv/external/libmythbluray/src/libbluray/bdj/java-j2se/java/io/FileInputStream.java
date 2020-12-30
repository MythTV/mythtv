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

import java.lang.reflect.InvocationTargetException;

import org.videolan.BDJLoader;
import org.videolan.BDJXletContext;
import org.videolan.Logger;
import org.videolan.MountManager;

public class FileInputStream extends InputStream
{
    private FileDescriptor fd;

    private final Object closeLock = new Object();
    private volatile boolean closed = false;

    private static Logger logger = null;

    private int available = 0;

    public FileInputStream(File file) throws FileNotFoundException {
        String name = file != null ? file.getPath() : null;

        /* Resolve resource urls obtained from URL.getPath(). */
        /* Ex. Aeon Flux, Petit Prince: file:/tmp/libbluray-bdj-cache/100d22e59d6d4/VFSCache/BDMV/JAR/00001.jar!/menu.bin */
        if (name != null && name.startsWith("file:")) {
            int idx;
            if ((idx = name.indexOf(".jar!/")) > 0 || (idx = name.indexOf(".jar!\\")) > 0) {
                try {
                    String mountPoint = MountManager.getMount(Integer.parseInt(name.substring(idx - 5, idx)));
                    if (mountPoint != null) {
                        name = mountPoint + name.substring(idx + 5);
                        getLogger().info("Fixing invalid resource url " + file + " --> " + name);
                        file = new File(name);
                    }
                } catch (Exception e) {
                    getLogger().error("" + e);
                }
            }
            if (name.startsWith("file:")) {
                System.err.println("ERROR: unresolved URL in FileInputStream: " + name);
            }
        }

        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            security.checkRead(name);
        }
        if (name == null) {
            throw new NullPointerException();
        }

        fd = new FileDescriptor();
        fdAttach();

        if (file.isAbsolute()) {
            String cachedName = BDJLoader.getCachedFile(name);
            if (cachedName != name) {
                getLogger().info("Using cached " + cachedName + " for " + name);
                name = cachedName;
            }
            openImpl(name);
        } else {
            /* relative paths are problematic ... */
            /* Those should be mapped to xlet home directory, which is inside .jar file. */

            String home = BDJXletContext.getCurrentXletHome();
            if (home == null) {
                getLogger().error("Xlet home directory not found for " + name + " at " + Logger.dumpStack());
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
        fd = fdObj;
        available = 1024;
        fdAttach();
    }

    /* open()/open0() wrapper to select correct native method at runtime */
    private void openImpl(String name) throws FileNotFoundException {
        try {
            /* OpenJDK 8 b40 */
            open0(name);
        } catch (UnsatisfiedLinkError e) {
            open(name);
        }
    }

    private native int  readBytes(byte b[], int off, int len) throws IOException;
    /* OpenJDK < 10 */
    private native int  close0();
    /* OpenJDK 6, OpenJDK 7, PhoneME, ... */
    private native void open(String name) throws FileNotFoundException;
    /* OpenJDK 8 */
    private native void open0(String name) throws FileNotFoundException;

    //public  native int  read() throws IOException;
    //public  native long skip(long n) throws IOException;
    //public  native int  available() throws IOException;

    public int available() throws IOException {
        return available;
    }

    public  int  read() throws IOException {
        byte b[] = new byte[1];
        if (read(b) == 1)
            return ((int)b[0]) & 0xff;
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

    private void close(boolean force) throws IOException {
        synchronized (closeLock) {
            if (closed) {
                return;
            }
            closed = true;
        }

        available = 0;

        fdClose(force);
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

    private static Logger getLogger() {
        synchronized (FileInputStream.class) {
            if (logger == null) {
                logger = Logger.getLogger(FileInputStream.class.getName());
            }
            return logger;
        }
    }

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

    /*
     * compat layer
     */

    private boolean useFdCount = false;

    private void fdAttach() {

        /*
         * Reflection fails at very early stage in JVM bootstrap.
         * -> hide all errors in this function.
         */
        try {
            try {
                fd.getClass().getDeclaredMethod("attach", new Class[] { Closeable.class })
                    .invoke(fd, new Object[] { (Object)this });
                return;
            } catch (NoSuchMethodException e) {
                /* older RT libs */
            }

            try {
                fd.getClass().getDeclaredMethod("incrementAndGetUseCount", new Class[0]).invoke((Object)fd, new Object[0]);
                useFdCount = true;
                return;
            } catch (NoSuchMethodException e) {
                getLogger().error("internal error in FileDescriptor usage");
            }

        } catch (Throwable t) {
            if (logger != null)
                logger.error("" + t);
        }
    }

    private void closeImpl() throws IOException {

        /* OpenJDK 10+ */
        try {
            fd.getClass().getDeclaredMethod("close", new Class[0])
                .invoke(fd, new Object[0]);
            return;
        } catch (InvocationTargetException ite) {
            Throwable t = ite.getTargetException();
            getLogger().error("" + t);
            if (t instanceof IOException) {
                throw (IOException)t;
            }
            throw new IOException();
        } catch (IllegalAccessException iae) {
            getLogger().error("internal error in FileDescriptor usage: " + iae);
            return;
        } catch (NoSuchMethodException no_jdk10) {
            /* JDK < 10 */
        }

        /* JDK < 10 */
        try {
            close0();
        } catch (UnsatisfiedLinkError no_close0) {
            getLogger().error("internal error in FileDescriptor usage: " + no_close0);
        }
    }

    private void fdClose(boolean force) throws IOException {

        try {

            if (useFdCount) {
                try {
                    Integer i = (Integer) fd.getClass().getDeclaredMethod("decrementAndGetUseCount", new Class[0]).invoke((Object)fd, new Object[0]);
                    if (i.intValue() > 0 && !force) {
                        return;
                    }
                    closeImpl();
                } catch (NoSuchMethodException no_method) {
                    getLogger().error("internal error in FileDescriptor usage: " + no_method);
                }
                return;
            }

            try {
                fd.getClass().getDeclaredMethod("closeAll", new Class[] { Closeable.class })
                    .invoke(fd, new Object[] { (Object)
                                               new Closeable() {
                                                   public void close() throws IOException {
                                                       closeImpl();
                                                   }
                                               }});
                return;
            } catch (NoSuchMethodException no_closeAll) {
                getLogger().error("internal error in FileDescriptor usage: " + no_closeAll);
            }

        } catch (IllegalAccessException iae) {
            getLogger().error("internal error in FileDescriptor usage: " + iae);
            return;
        } catch (InvocationTargetException ite) {
            Throwable t = ite.getTargetException();
            getLogger().error("" + t);
            if (t instanceof IOException) {
                throw (IOException)t;
            }
            throw new IOException();
        }
    }
}
