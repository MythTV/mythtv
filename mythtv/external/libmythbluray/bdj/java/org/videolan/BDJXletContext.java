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

import java.awt.Container;
import java.security.AccessController;
import java.security.PrivilegedAction;

import javax.microedition.xlet.UnavailableContainerException;

import org.dvb.application.AppID;
import org.dvb.application.AppProxy;
import org.dvb.application.AppsDatabase;
import org.videolan.bdjo.AppCache;
import org.videolan.bdjo.AppEntry;

public class BDJXletContext implements javax.tv.xlet.XletContext, javax.microedition.xlet.XletContext {
    public BDJXletContext(AppEntry entry, AppCache[] caches, Container container) {
        this.appid = entry.getIdentifier();
        this.args = entry.getParams();
        this.loader = BDJClassLoader.newInstance(
                caches,
                entry.getBasePath(),
                entry.getClassPathExt(),
                entry.getInitialClass());
        this.container = container;
    }

    public Object getXletProperty(String key) {
        if (key.equals(javax.tv.xlet.XletContext.ARGS) ||
            key.equals(javax.microedition.xlet.XletContext.ARGS))
            return args;
        else if (key.equals("dvb.org.id"))
            return Integer.toHexString(appid.getOID());
        else if (key.equals("dvb.app.id"))
            return Integer.toHexString(appid.getAID());
        else if (key.equals("org.dvb.application.appid"))
            return appid;
        return null;
    }

    public void notifyDestroyed() {
        AppProxy proxy = AppsDatabase.getAppsDatabase().getAppProxy(appid);
        if (proxy instanceof BDJAppProxy)
            ((BDJAppProxy)proxy).notifyDestroyed();
    }

    public void notifyPaused() {
        AppProxy proxy = AppsDatabase.getAppsDatabase().getAppProxy(appid);
        if (proxy instanceof BDJAppProxy)
            ((BDJAppProxy)proxy).notifyPaused();
    }

    public void resumeRequest() {
        AppProxy proxy = AppsDatabase.getAppsDatabase().getAppProxy(appid);
        if (proxy instanceof BDJAppProxy)
            ((BDJAppProxy)proxy).resume();
    }

    public Container getContainer() throws UnavailableContainerException {
        return container;
    }

    public ClassLoader getClassLoader() {
        return loader;
    }

    public static BDJXletContext getCurrentContext() {
        Object obj = AccessController.doPrivileged(
                new PrivilegedAction<Object>() {
                    public Object run() {
                        ThreadGroup group = Thread.currentThread().getThreadGroup();
                        while ((group != null) && !(group instanceof BDJThreadGroup))
                            group = group.getParent();
                        return group;
                    }
                }
            );
        if (obj == null)
            return null;
        return ((BDJThreadGroup)obj).getContext();
    }

    protected void setArgs(String[] args) {
        this.args = args;
    }

    protected void update(AppEntry entry, AppCache[] caches) {
        args = entry.getParams();
        loader.update(
                caches,
                entry.getBasePath(),
                entry.getClassPathExt(),
                entry.getInitialClass());
    }

    private String[] args;
    private AppID appid;
    private BDJClassLoader loader;
    private Container container;
}
