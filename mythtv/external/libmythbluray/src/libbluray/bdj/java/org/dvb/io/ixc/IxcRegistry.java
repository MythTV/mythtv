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

package org.dvb.io.ixc;

import java.rmi.RemoteException;
import java.rmi.NotBoundException;
import java.rmi.AlreadyBoundException;
import java.rmi.Remote;

import javax.tv.xlet.XletContext;

import org.videolan.IxcRegistryImpl;


public class IxcRegistry {

    private static final Object instanceLock = new Object();

    private static IxcRegistryImpl registry = null;

    private static IxcRegistryImpl getIxcRegistry() {
        synchronized (instanceLock) {
            if (registry == null) {
                registry = new IxcRegistryImpl();
            }
            return registry;
        }
    }

    public static void shutdown() {
        IxcRegistryImpl r;
        synchronized (instanceLock) {
            r = registry;
            registry = null;
        }
        if (r != null) {
            r.unbindAll();
        }
    }

    public static Remote lookup(XletContext xc, String path) throws NotBoundException, RemoteException {
        int orgid, appid;
        int s1, s2;
        if (path.charAt(0) != '/')
            throw new IllegalArgumentException();
        s1 = path.indexOf('/', 1);
        if (s1 <= 1)
            throw new IllegalArgumentException();
        try {
            orgid = Integer.parseInt(path.substring(1, s1), 16);
        } catch (Exception e) {
            throw new IllegalArgumentException();
        }
        s1++;
        s2 = path.indexOf('/', s1);
        if (s2 <= s1)
            throw new IllegalArgumentException();
        try {
            appid = Integer.parseInt(path.substring(s1, s2), 16);
        } catch (Exception e) {
            throw new IllegalArgumentException();
        }
        String key = "/" + Integer.toHexString(orgid) +
                     "/" + Integer.toHexString(appid) +
                     "/" + path.substring(s2 + 1, path.length());

        return getIxcRegistry().lookup(xc, key);
    }

    public static void bind(XletContext xc, String name, Remote obj) throws AlreadyBoundException {
        if ((xc == null) || (name == null) || (obj == null))
            throw new NullPointerException();
        String key = "/" + (String)xc.getXletProperty("dvb.org.id") +
                     "/" + (String)xc.getXletProperty("dvb.app.id") +
                     "/" + name;

        getIxcRegistry().bind(xc, key, obj);
    }

    public static void unbind(XletContext xc, String name) throws NotBoundException {
        if ((xc == null) || (name == null))
            throw new NullPointerException();
        String key = "/" + (String)xc.getXletProperty("dvb.org.id") +
                     "/" + (String)xc.getXletProperty("dvb.app.id") +
                     "/" + name;

        getIxcRegistry().unbind(key);
    }

    public static void rebind(XletContext xc, String name, Remote obj) {
        if ((xc == null) || (name == null) || (obj == null))
            throw new NullPointerException();
        String key = "/" + (String)xc.getXletProperty("dvb.org.id") +
                     "/" + (String)xc.getXletProperty("dvb.app.id") +
                     "/" + name;
        try {
            getIxcRegistry().unbind(key);
        }
        catch (NotBoundException e) {
        }
        try {
            getIxcRegistry().bind(xc, key, obj);
        } catch (AlreadyBoundException e) {
        }
    }

    public static String[] list(XletContext xc) {
        return getIxcRegistry().list();
    }

    public static void unbindAll(XletContext xc) {
        getIxcRegistry().unbindAll(xc);
    }
}
