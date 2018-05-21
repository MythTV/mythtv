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

import javax.tv.xlet.Xlet;

import org.videolan.bdjo.AppCache;

public class BDJClassLoader extends URLClassLoader {
    public static BDJClassLoader newInstance(AppCache[] appCaches, String basePath, String classPathExt, String xletClass) {
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
        return new BDJClassLoader((URL[])classPath.toArray(new URL[classPath.size()]) , xletClass);
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
        String protocol = null;
        String url = path.substring(0, 5);
        for (int i = 0; i < appCaches.length; i++) {
            if (appCaches[i].getRefToName().equals(url)) {
                if (appCaches[i].getType() == AppCache.JAR_FILE) {
                    protocol = "file:";
                    url = url + ".jar";
                } else if (appCaches[i].getType() == AppCache.DIRECTORY) {
                    protocol = "file:/";
                }
                break;
            }
        }
        if (protocol == null)
            return null;
        url = protocol +
            BDJLoader.getCachedFile(System.getProperty("bluray.vfs.root") + File.separator +
                                    "BDMV" + File.separator +
                                    "JAR" + File.separator +
                                    url) + path.substring(5);
        //if (!url.endsWith("/"))
        //    url += "/";
        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            System.err.println("" + e + "\n" + Logger.dumpStack(e));
            return null;
        }
    }

    private BDJClassLoader(URL[] urls, String xletClass) {
        super(urls);
        this.xletClass = xletClass;
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

    public Class loadClass(String name) throws java.lang.ClassNotFoundException {
        /* hook FileSystem in java.io.File */
        if (name.equals("java.io.File")) {
            Class c = super.loadClass(name);
            if (c != null) {
                java.io.BDFileSystem.init(c);
            }
            return c;
        }

        try {
            return super.loadClass(name);
        } catch (ClassNotFoundException e0) {
            logger.error("ClassNotFoundException: " + name);
            throw e0;
        } catch (Error err) {
            logger.error("FATAL: " + err);
            throw err;
        }
    }

    private byte[] loadClassCode(String name) throws ClassNotFoundException {
        String path = name.replace('.', '/').concat(".class");

        URL res = super.findResource(path);
        if (res == null) {
            logger.error("loadClassCode(): resource for class " + name + "not found");
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
                if (r == -1) break;
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
                b = new BDJClassFileTransformer().transform(b, 0, b.length);
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
        name = name.replace('\\', '/');
        return super.getResource(name);
    }

    /* final in J2ME
    public Enumeration getResources(String name) throws IOException {
        name = name.replace('\\', '/');
        return super.getResources(name);
    }
    */

    public URL findResource(String name) {
        name = name.replace('\\', '/');
        return super.findResource(name);
    }

    public Enumeration findResources(String name) throws IOException {
        name = name.replace('\\', '/');
        return super.findResources(name);
    }

    public InputStream getResourceAsStream(String name) {
        name = name.replace('\\', '/');
        return super.getResourceAsStream(name);
    }

    private String xletClass;

    private static final Logger logger = Logger.getLogger(BDJClassLoader.class.getName());
}
