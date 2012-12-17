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
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;

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
            System.getProperty("bluray.vfs.root") +
            "/BDMV/JAR/" + url + path.substring(5);
        //if (!url.endsWith("/"))
        //    url += "/";
        try {
            return new URL(url);
        } catch (MalformedURLException e) {
            e.printStackTrace();
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

    private String xletClass;
}
