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

package org.videolan;

import java.util.Enumeration;
import java.util.Vector;

import org.dvb.application.AppAttributes;
import org.dvb.application.AppID;
import org.dvb.application.AppProxy;
import org.dvb.application.AppsDatabase;
import org.dvb.application.AppsDatabaseEvent;
import org.dvb.application.AppsDatabaseFilter;
import org.videolan.bdjo.AppEntry;
import org.videolan.bdjo.Bdjo;

public class BDJAppsDatabase extends AppsDatabase {

    private static final Object instanceLock = new Object();

    static public AppsDatabase getAppsDatabase() {
        synchronized (instanceLock) {
            if (instance == null)
                instance = new BDJAppsDatabase();
            return instance;
        }
    }

    private static final Object appTableLock = new Object();

    public int size() {
        synchronized (appTableLock) {
            if (appTable == null)
                return 0;
            return appTable.length;
        }
    }

    public Enumeration getAppIDs(AppsDatabaseFilter filter) {
        Vector ids = new Vector();
        synchronized (appTableLock) {
            if (appTable != null)
                for (int i = 0; i < appTable.length; i++)
                    if (filter.accept(appTable[i].getIdentifier()))
                        ids.add(appTable[i].getIdentifier());
        }
        return ids.elements();
    }

    public Enumeration getAppAttributes(AppsDatabaseFilter filter) {
        Vector attributes = new Vector();
        synchronized (appTableLock) {
            if (appTable != null)
                for (int i = 0; i < size(); i++)
                    if (filter.accept(appTable[i].getIdentifier()))
                        attributes.add(appTable[i]);
        }
        return attributes.elements();
    }

    public AppAttributes getAppAttributes(AppID key) {
        synchronized (appTableLock) {
            if (appTable != null)
                for (int i = 0; i < size(); i++)
                    if (key.equals(appTable[i].getIdentifier()))
                        return appTable[i];
        }
        return null;
    }

    public AppProxy getAppProxy(AppID key) {
        synchronized (appTableLock) {
            if ((appTable != null) && (appProxys != null))
                for (int i = 0; i < size(); i++)
                    if (key.equals(appTable[i].getIdentifier()))
                        return appProxys[i];
        }
        return null;
    }

    public Bdjo getBdjo() {
        return bdjo;
    }

    protected void newDatabase(Bdjo bdjo, BDJAppProxy[] appProxys) {
        synchronized (appTableLock) {
            this.bdjo = bdjo;
            this.appProxys = appProxys;
            this.appTable = (bdjo != null) ? bdjo.getAppTable() : null;
        }
        notifyListeners(AppsDatabaseEvent.NEW_DATABASE, null);
    }

    private Bdjo bdjo = null;
    private BDJAppProxy[] appProxys = null;
    private AppEntry[] appTable = null;

    private static BDJAppsDatabase instance = null;
}
