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

public interface AppProxy {
    public static final int STARTED = 0;
    public static final int DESTROYED = 1;
    public static final int NOT_LOADED = 2;
    public static final int PAUSED = 3;
    public static final int INVALID = 8;

    public int getState();

    public void start();

    public void start(String args[]);

    public void stop(boolean force);

    public void pause();

    public void resume();

    public void addAppStateChangeEventListener(
            AppStateChangeEventListener listener);

    public void removeAppStateChangeEventListener(
            AppStateChangeEventListener listener);

}
