/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package org.bluray.bdplus;

import org.videolan.BDJListeners;
import org.videolan.Libbluray;
import org.videolan.Logger;

public class Status {

    private static final Object instanceLock = new Object();

    public static Status getInstance() {
        synchronized (instanceLock) {
            if (instance == null)
                instance = new Status();
            return instance;
        }
    }

    public static void shutdown() {
        Status s;
        synchronized (instanceLock) {
            s = instance;
            instance = null;
        }
        if (s != null) {
            s.listeners.clear();
        }
    }

    public void addListener(StatusListener listener) {
        listeners.add(listener);
    }

    public int get() {
        int val = Libbluray.readPSR(104);
        logger.trace("get(): 0x" + Integer.toHexString(val));
        return val;
    }

    public void removeListener(StatusListener listener) {
        listeners.remove(listener);
    }

    public void send(int data) {
        logger.trace("send(0x" + Integer.toHexString(data) + ")");
        Libbluray.writePSR(103, data);
    }

    public void set(int data) {
        logger.trace("set(0x" + Integer.toHexString(data) + ")");
        Libbluray.writePSR(104, data);
    }

    public void receive(int data) {
        logger.trace("receive(0x" + Integer.toHexString(data) + ")");
        listeners.putPSR102Callback(data);
    }

    private static Status instance = null;
    private BDJListeners listeners = new BDJListeners();

    private static final Logger logger = Logger.getLogger(Status.class.getName());
}
