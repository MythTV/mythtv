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

package org.dvb.application;

import java.util.LinkedList;
import java.util.Enumeration;
import java.util.Collections;
import org.videolan.BDJAppsDatabase;
import org.videolan.BDJListeners;
import org.videolan.Logger;

public class AppsDatabase {
    static public AppsDatabase getAppsDatabase() {
        return BDJAppsDatabase.getAppsDatabase();
    }

    public int size() {
        logger.unimplemented("size");
        return 0;
    }

    public Enumeration getAppIDs(AppsDatabaseFilter filter) {
        logger.unimplemented("getAppIDs");
        return Collections.emptyEnumeration();
    }

    public Enumeration getAppAttributes(AppsDatabaseFilter filter) {
        logger.unimplemented("getAppAttributes");
        return Collections.emptyEnumeration();
    }

    public AppAttributes getAppAttributes(AppID key) {
        logger.unimplemented("getAppAttributes");
        return null;
    }

    public AppProxy getAppProxy(AppID key) {
        logger.unimplemented("getAppProxy");
        return null;
    }

    public void addListener(AppsDatabaseEventListener listener) {
        listeners.add(listener);
    }

    public void removeListener(AppsDatabaseEventListener listener) {
        listeners.remove(listener);
    }

    protected void notifyListeners(int id, AppID appid) {
        listeners.putCallback(new AppsDatabaseEvent(id, appid, this));
    }

    private BDJListeners listeners = new BDJListeners();
    private static final Logger logger = Logger.getLogger(AppsDatabase.class.getName());
}
