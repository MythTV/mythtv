/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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

import java.net.MalformedURLException;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Map;

import java.security.AccessController;
import java.security.PrivilegedAction;

import javax.tv.xlet.Xlet;

import org.videolan.bdjo.AppCache;

class BDJClassLoader extends URLClassLoader {
    public static BDJClassLoader newInstance(AppCache[] appCaches, String basePath, String classPathExt, final String xletClass,
                                             final BDJClassFilePatcher patcher) {
        ArrayList classPath = new ArrayList();
        URL url = translateClassPath(appCaches, basePath, null);
        if (url != null)
            classPath.add(url);
        String[] classPaths = StrUtil.split(classPathExt, ';');
        for (int i = 0; i < classPaths.length; i++) {
            url = translateClassPath(appCaches, basePath, classPaths[i]);
            if ((url != null) && (classPath.indexOf(url) < 0))
                classPath.add(url);
        }

        final URL[] urls = (URL[])classPath.toArray(new URL[classPath.size()]);
        return (BDJClassLoader)AccessController.doPrivileged(
                new PrivilegedAction() {
                    public Object run() {
                        return new BDJClassLoader(urls, xletClass, patcher);
                    }
                });
    }

    private static URL translateClassPath(AppCache[] appCaches, String basePath, String classPath) {
        String path;
        if ((classPath == null) || (classPath.length() <= 0))
            path = basePath;
        else if (classPath.charAt(0) == '/')
            path = classPath.substring(1);
        else
            path = basePath + "/" + classPath;
        if (path.length() < 5)
            return null;

        String name = path.substring(0, 5); /* entry name in BDMV/JAR/ */
        String dir  = path.substring(5);    /* optional sub directory */

        /* find bdjo AppCache entry */
        int i;
        for (i = 0; i < appCaches.length; i++) {
            if (appCaches[i].getRefToName().equals(name)) {
                break;
            }
        }
        if (i >= appCaches.length)
            return null;

        /* check AppCache type */
        String protocol;
        if (appCaches[i].getType() == AppCache.JAR_FILE) {
            protocol = "file:";
            name = name + ".jar";
        } else if (appCaches[i].getType() == AppCache.DIRECTORY) {
            protocol = "file:/";
        } else {
            return null;
        }

        /* map to disc root */
        String fullPath =
            System.getProperty("bluray.vfs.root") + File.separator +
            "BDMV" + File.separator + "JAR" + File.separator +
            name;

        /* find from cache */
        String cachePath = BDJLoader.getCachedFile(fullPath);

        /* build final url */
        String url = protocol + cachePath + dir;

        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            System.err.println("" + e + "\n" + Logger.dumpStack(e));
            return null;
        }
    }

    private BDJClassLoader(URL[] urls, String xletClass, BDJClassFilePatcher patcher) {
        super(urls);
        this.xletClass = xletClass;

        BDJClassLoaderAdapter a = Libbluray.getClassLoaderAdapter();
        if (a != null) {
            hideClasses = a.getHideClasses();
            bootClasses = a.getBootClasses();
            xletClasses = a.getXletClasses();
        }
        this.patcher = patcher;
    }

    protected Xlet loadXlet() throws ClassNotFoundException,
            IllegalAccessException, InstantiationException {
        return (Xlet)loadClass(xletClass).newInstance();
    }

    protected void update(AppCache[] appCaches, String basePath, String classPathExt, String xletClass) {
        ArrayList classPath = new ArrayList();
        URL[] urls = getURLs();
        for (int i = 0; i < urls.length; i++)
            classPath.add(urls[i]);
        URL url = translateClassPath(appCaches, basePath, null);
        if (url != null)
            classPath.add(url);
        String[] classPaths = StrUtil.split(classPathExt, ';');
        for (int i = 0; i < classPaths.length; i++) {
            url = translateClassPath(appCaches, basePath, classPaths[i]);
            if ((url != null) && (classPath.indexOf(url) < 0))
                classPath.add(url);
        }
        for (int i = 0; i < classPath.size(); i++)
            addURL((URL)classPath.get(i));
        this.xletClass = xletClass;
    }

    public Class loadClass(String name) throws ClassNotFoundException {

        if (hideClasses != null) {
            if (hideClasses.get(name) != null) {
                logger.error("Hiding class " + name);
                throw new ClassNotFoundException(name);
            }
        }

        /* hook FileSystem in java.io.File */
        if (name.equals("java.io.File")) {
            Class c = super.loadClass(name);
            if (c != null) {
                java.io.BDFileSystem.init(c);
            }
            return c;
        }

        Class bootclass = tryLoad(name, bootClasses);
        if (bootclass != null) {
            return bootclass;
        }

        try {
            return super.loadClass(name);
        } catch (ClassNotFoundException e0) {
            logger.error("ClassNotFoundException: " + name);

            Class xletclass = tryLoad(name, xletClasses);
            if (xletclass != null) {
                return xletclass;
            }

            throw e0;
        } catch (Error err) {
            logger.error("FATAL: " + err);
            throw err;
        }
    }

    private Class tryLoad(String name, Map classes) {
        if (classes != null) {
            try {
                byte[] code = (byte[])classes.get(name);
                if (code != null) {
                    logger.info("Overriding class " + name);
                    return defineClass(code, 0, code.length);
                }
            } catch (Exception e) {
                logger.error("tryLoad(" + name + ") failed: " + e);
            }
        }
        return null;
    }

    private byte[] loadClassCode(String name) throws ClassNotFoundException {
        String path = name.replace('.', '/').concat(".class");

        URL res = super.findResource(path);
        if (res == null) {
            logger.error("loadClassCode(): resource for class " + name + " not found");
            throw new ClassNotFoundException(name);
        }

        InputStream is = null;
        ByteArrayOutputStream os = null;
        try {
            is = res.openStream();
            os = new ByteArrayOutputStream();
            byte[] buffer = new byte[0xffff];
            while (true) {
                int r = is.read(buffer);
                if (r == -1) {
                    break;
                }
                os.write(buffer, 0, r);
            }

            return os.toByteArray();

        } catch (Exception e) {
            logger.error("loadClassCode(" + name + ") failed: " + e);
            throw new ClassNotFoundException(name);

        } finally {
            try {
                if (is != null)
                    is.close();
            } catch (IOException ioe) {
            }
            try {
                if (os != null)
                    os.close();
            } catch (IOException ioe) {
            }
        }
    }

    protected Class findClass(String name) throws ClassNotFoundException {

        if (patcher != null) {
            try {
                byte[] b = loadClassCode(name);
                b = patcher.patch(b);
                return defineClass(b, 0, b.length);
            } catch (ThreadDeath td) {
                throw td;
            } catch (Throwable t) {
                logger.error("Class patching failed: " + t);
            }
        }

        try {
            return super.findClass(name);

        } catch (ClassFormatError ce) {

            /* try to "fix" broken class file */
            /* if we got ClassFormatError, package was already created. */
            byte[] b = loadClassCode(name);
            if (b == null) {
                logger.error("loadClassCode(" + name + ") failed");
                /* this usually kills Xlet ... */
                throw ce;
            }
            try {
                b = new BDJClassFileTransformer().strip(b, 0, b.length);
                return defineClass(b, 0, b.length);
            } catch (ThreadDeath td) {
                throw td;
            } catch (Throwable t) {
                logger.error("Class rewriting failed: " + t);
                throw new ClassNotFoundException(name);
            }

        } catch (Error er) {
            logger.error("Unexpected error: " + er + " " + Logger.dumpStack(er));
            throw er;
        }
    }

    public URL getResource(String name) {
        URL url;
        name = name.replace('\\', '/');
        url = super.getResource(name);
        logger.info("getResource(" + name + ") --> " + url);
        return url;
    }

    /* final in J2ME
    public Enumeration getResources(String name) throws IOException {
        name = name.replace('\\', '/');
        return super.getResources(name);
    }
    */

    public URL findResource(String name) {
        URL url;
        name = name.replace('\\', '/');
        url = super.findResource(name);
        logger.info("findResource(" + name + ") --> " + url);
        return url;
    }

    public Enumeration findResources(String name) throws IOException {
        name = name.replace('\\', '/');
        return super.findResources(name);
    }

    public InputStream getResourceAsStream(String name) {
        InputStream is;
        name = name.replace('\\', '/');
        is = super.getResourceAsStream(name);
        if (is == null) {
            logger.info("getResourceAsStream(" + name + ") failed");
        }
        return is;
    }

    private String xletClass;

    private final BDJClassFilePatcher patcher;
    private Map hideClasses;  /* classes that should be hidden from Xlet */
    private Map bootClasses;  /* additional bootstrap classes */
    private Map xletClasses;  /* fallback for possibly missing classes */

    private static final Logger logger = Logger.getLogger(BDJClassLoader.class.getName());
}
