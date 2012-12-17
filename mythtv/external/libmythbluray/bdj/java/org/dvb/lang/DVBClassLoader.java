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
package org.dvb.lang;

import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.util.Enumeration;
import java.util.jar.Manifest;

public abstract class DVBClassLoader extends java.security.SecureClassLoader {
    public static DVBClassLoader newInstance(URL[] urls) {
        return new DVBClassLoaderImpl(urls);
    }

    public static DVBClassLoader newInstance(URL[] urls, ClassLoader parent) {
        return new DVBClassLoaderImpl(urls, parent);
    }

    public DVBClassLoader(URL[] urls) {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null)
            sm.checkCreateClassLoader();
        loader = new DVBURLClassLoader(urls);
    }

    public DVBClassLoader(URL[] urls, ClassLoader parent) {
        super(parent);
        SecurityManager sm = System.getSecurityManager();
        if (sm != null)
            sm.checkCreateClassLoader();
        loader = new DVBURLClassLoader(urls, parent);
    }

    public Class<?> findClass(String name) throws ClassNotFoundException {
        return loader.findClass(name);
    }

    protected Package definePackage(String name, Manifest man, URL url)
            throws IllegalArgumentException {
        return loader.definePackage(name, man, url);
    }

    protected URL findResource(String name) {
        return loader.findResource(name);
    }

    protected Enumeration findResources(String name) throws IOException {
        return loader.findResources(name);
    }

    protected PermissionCollection getPermissions(CodeSource codesource) {
        return loader.getPermissions(codesource);
    }

    protected Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
        return loader.loadClass(name, resolve);
    }

    protected class DVBURLClassLoader extends URLClassLoader {
        public DVBURLClassLoader(URL[] urls) {
            super(urls);
        }

        public DVBURLClassLoader(URL[] urls, ClassLoader parent) {
            super(urls, parent);
        }

        protected Class findClass(String name) throws ClassNotFoundException {
            return super.findClass(name);
        }

        protected Package definePackage(String name, Manifest man, URL url)
                        throws IllegalArgumentException {
            return super.definePackage(name, man, url);
        }

        protected PermissionCollection getPermissions(CodeSource codesource) {
            return super.getPermissions(codesource);
        }

        protected Class loadClass(String name, boolean resolve) throws ClassNotFoundException {
            return super.loadClass(name, resolve);
        }
    }

    protected DVBURLClassLoader loader;
}
