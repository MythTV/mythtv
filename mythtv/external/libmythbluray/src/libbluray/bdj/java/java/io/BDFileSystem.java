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

/*
 * Wrapper for java.io.FileSystem class.
 *
 * - resolve relative files to Xlet home directory.
 * - replace getBooleanAttributes() for relative paths.
 *   Pretend files exist, if those are in xlet home directory (inside .jar).
 *   No other relative paths are allowed.
 */

package java.io;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

import java.security.AccessController;
import java.security.PrivilegedAction;

import org.videolan.BDJLoader;
import org.videolan.BDJXletContext;
import org.videolan.Logger;

public abstract class BDFileSystem extends FileSystem {

    /*
     * Access to native filesystem
     *
     * (for org.videolan.VFSCache, org.videolan.CacheDir)
     */

    private static FileSystem nativeFileSystem;

    static {
        try {
            /* Workaround for Java 9 bootstrap issues:
             * In some Java 9 versions this methods is called in VM init phase1
             * before setJavaLangAccess() -> getDeclaredMethod() crashes in StringJoiner
             */
            String v = System.getProperty("java.version");
            int i = 0;
            if (v.startsWith("1."))
                v = v.substring(2, 3);
            if (v.indexOf(".") != -1)
                v = v.substring(0, v.indexOf("."));
            if (v.indexOf("-") != -1)
                v = v.substring(0, v.indexOf("-"));
            try {
                i = Integer.parseInt(v);
            } catch (Throwable t) {
                i = 0;
            }
            if (i >= 8)
                throw new Exception("java>7");

            /* Java < 8 */
            nativeFileSystem = (FileSystem)FileSystem.class
                .getDeclaredMethod("getFileSystem",new Class[0])
                .invoke(null, new Object[0]);
        } catch (Exception e) {
            try {
                /* Just use our wrapper.  If it fails, JVM won't be booted anyway ... */
                nativeFileSystem = DefaultFileSystem.getNativeFileSystem();
            } catch (Throwable t) {
                System.err.print("Couldn't find native filesystem: " + e);
            }
        }
    }

    /* org.videolan.CacheDir uses this function to clean up cache directory */
    public static String[] nativeList(File f) {
        return nativeFileSystem.list(f);
    }

    /* org.videolan.VFSCache uses this function to check if file has been cached */
    public static boolean nativeFileExists(String path) {
        return nativeFileSystem.getBooleanAttributes(new File(path)) != 0;
    }

    /*
     * Replace File.fs for Xlets (required with Java < 8 where this is not done unconditionally)
     *
     * (called by org.videolan.BDJClassLoader)
     */

    public static void init(final Class c) {

        setBooted();

        AccessController.doPrivileged(
            new PrivilegedAction() {
                public Object run() {
                    init0(c);
                    return null;
                }
            });
    }

    private static void init0(Class c) {
        Field filesystem;
        try {
            filesystem = c.getDeclaredField("fs");
            filesystem.setAccessible(true);

            FileSystem fs = (FileSystem)filesystem.get(null);
            if (fs instanceof BDFileSystemImpl) {
                //System.err.print("FileSystem already wrapped");
            } else {
                /* Java 8: we should never end up here ... */
                /* Java 8: remove "final" modifier from the field */
                //Field modifiersField = Field.class.getDeclaredField("modifiers");
                //modifiersField.setAccessible(true);
                //modifiersField.setInt(filesystem, filesystem.getModifiers() & ~Modifier.FINAL);
                filesystem.set(null, new BDFileSystemImpl(fs));
            }
        } catch (Exception t) {
            error("Hooking FileSystem class failed: " + t);
        }
    }

    /*
     * enable after JVM boot is completed
     */

    private static Logger logger = null;
    private static boolean booted = false;

    /* Called by org.videolan.Libbluray.initOnce() */
    public static void setBooted() {
        if (!booted) {
            booted = true;
            logger = Logger.getLogger(BDFileSystem.class.getName());
        }
    }

    private static void error(String msg) {
        if (logger != null) {
            logger.error(msg);
        }
    }

    /*
     *
     */

    protected final FileSystem fs;

    public BDFileSystem(FileSystem fs) {
        this.fs = fs;
    }

    public char getSeparator() {
        return fs.getSeparator();
    }

    public char getPathSeparator() {
        return fs.getPathSeparator();
    }

    public String normalize(String pathname) {
        return fs.normalize(pathname);
    }

    public int prefixLength(String pathname) {
        return fs.prefixLength(pathname);
    }

    public static boolean isAbsolutePath(String path) {
        return path.startsWith("/") || path.indexOf(":\\") == 1 ||
            path.startsWith("\\");
    }

    private String getHomeDir() {
        String home = BDJXletContext.getCurrentXletHome();
        if (home == null)
            return "";
        return home;
    }

    public String resolve(String parent, String child) {
        if (!booted)
            return fs.resolve(parent, child);

        if (parent == null || parent.equals("") || parent.equals(".")) {
            parent = getHomeDir();
        }
        else if (!isAbsolutePath(parent)) {
            logger.info("resolve relative file at " + parent);
            parent = getHomeDir() + parent;
        }

        String resolvedPath = fs.resolve(parent, child);
        String cachePath = BDJLoader.getCachedFile(resolvedPath);
        if (cachePath != resolvedPath && !cachePath.equals(resolvedPath)) {
            logger.info("resolve(p,c): using cached " + cachePath + " (" + resolvedPath + ")");
        }
        return cachePath;
    }

    public String getDefaultParent() {
        return fs.getDefaultParent();
    }

    public String fromURIPath(String path) {
        return fs.fromURIPath(path);
    }

    public boolean isAbsolute(File f) {
        return fs.isAbsolute(f);
    }

    public boolean isInvalid(File f) {
        try {
            Method m = fs.getClass().getDeclaredMethod("isInvalid", new Class[] { File.class });
            Object[] args = new Object[] {(Object)f};
            Boolean result = (Boolean)m.invoke(fs, args);
            return result.booleanValue();
        } finally {
            return false;
        }
    }

    public String resolve(File f) {
        if (!booted)
            return fs.resolve(f);

        if (!f.isAbsolute()) {
            logger.info("resolve relative file " + f.getPath());
            return resolve(BDJXletContext.getCurrentXletHome(), f.getPath());
        }

        String resolvedPath = fs.resolve(f);
        String cachePath = BDJLoader.getCachedFile(resolvedPath);
        if (cachePath != resolvedPath && !cachePath.equals(resolvedPath)) {
            logger.info("resolve(f): using cached " + cachePath + " (" + resolvedPath + ")");
        }
        return cachePath;
    }

    public String canonicalize(String path) throws IOException {
        if (!booted)
            return fs.canonicalize(path);

        String canonPath = fs.canonicalize(path);
        String cachePath = BDJLoader.getCachedFile(canonPath);
        if (cachePath != canonPath && !cachePath.equals(canonPath)) {
            logger.info("canonicalize(): Using cached " + cachePath + " for " + canonPath + "(" + path + ")");
        }
        return cachePath;
    }

    public int getBooleanAttributes(File f) {
        if (!booted)
            return fs.getBooleanAttributes(f);

        if (f.isAbsolute()) {
            return fs.getBooleanAttributes(f);
        }

        /* try to locate file in Xlet home directory */
        String home = BDJXletContext.getCurrentXletHome();
        if (home == null) {
            logger.error("no home found for " + f.getPath() + " at " + Logger.dumpStack());
            return 0;
        }

        String path = home + f.getPath();
        logger.info("Relative path " + f.getPath() + " translated to " + path);
        return fs.getBooleanAttributes(new File(path));
    }

    /*
      ME: public abstract boolean checkAccess(File f, boolean write);
      SE: public abstract boolean checkAccess(File f, int access);
    */

    public long getLastModifiedTime(File f) {
        return fs.getLastModifiedTime(f);
    }

    public long getLength(File f) {
        if (!booted)
            return fs.getLength(f);

        if (f.isAbsolute()) {
            return fs.getLength(f);
        }

        /* try to locate file in Xlet home directory */
        String home = BDJXletContext.getCurrentXletHome();
        if (home == null) {
            logger.error("no home found for " + f.getPath() + " at " + Logger.dumpStack());
            return 0;
        }

        String path = home + f.getPath();
        logger.info("Relative path " + f.getPath() + " translated to " + path);
        return fs.getLength(new File(path));
    }

    /*
      SE only
      public abstract boolean setPermission(File f, int access, boolean enable, boolean owneronly);
    */

    /* this version exists in some java6 versions.
     * Use reflection to make sure build succees and right method is called.
     */
    public boolean createFileExclusively(String path, boolean restrictive) throws IOException {
        return createFileExclusivelyImpl(path, restrictive);
    }
    /* this version exists in most java versions (1.4, 1.7, some 1.6 versions) */
    public boolean createFileExclusively(String path) throws IOException {
        return createFileExclusivelyImpl(path, false);
    }

    private boolean createFileExclusivelyImpl(String path, boolean restrictive) throws IOException {
        Method m;
        Object[] args;

        /* resolve method and set up arguments */
        try {
            try {
                m = fs.getClass().getDeclaredMethod("createFileExclusively", new Class[] { String.class });
                args = new Object[] {(Object)path};
            } catch (NoSuchMethodException e) {
                m  = fs.getClass().getDeclaredMethod("createFileExclusively", new Class[] { String.class, boolean.class });
                args = new Object[] {(Object)path, (Object)new Boolean(restrictive)};
            }
        } catch (NoSuchMethodException e) {
            error("no matching FileSystem.createFileExclusively found !");
            throw new IOException();
        }

        /* call */
        try {
            Boolean result = (Boolean)m.invoke(fs, args);
            return result.booleanValue();
        } catch (IllegalAccessException e) {
            error("" + e);
            throw new IOException();
        } catch (InvocationTargetException e) {
            Throwable t = e.getTargetException();
            if (t instanceof IOException) {
                throw (IOException)t;
            }
            error("" + t);
            throw new IOException();
        }
    }

    /*
      ME only
      public abstract boolean deleteOnExit(File f);
    */

    /*
      SE only
      public abstract long getSpace(File f, int t);
    */

    public boolean delete(File f) {
        return fs.delete(f);
    }

    public String[] list(File f) {
        if (!booted)
            return fs.list(f);

        String path = f.getPath();
        String root = System.getProperty("bluray.vfs.root");
        if (root == null || !path.startsWith(root)) {
            /* not inside VFS */
            return fs.list(f);
        }

        /* path is inside VFS */
        /* EX. HOSTEL_2 lists files in BD-ROM */
        int rootLength = root.length();
        path = path.substring(rootLength);

        String[] names = org.videolan.Libbluray.listBdFiles(path, false);
        return names;
    }

    public boolean createDirectory(File f) {
        return fs.createDirectory(f);
    }

    public boolean rename(File f1, File f2) {
        return fs.rename(f1, f2);
    }

    public boolean setLastModifiedTime(File f, long time) {
        return fs.setLastModifiedTime(f, time);
    }

    public boolean setReadOnly(File f) {
        return fs.setReadOnly(f);
    }

    public File[] listRoots() {
        return fs.listRoots();
    }

    public int compare(File f1, File f2) {
        return fs.compare(f1, f2);
    }

    public int hashCode(File f) {
        return fs.hashCode(f);
    }
}
