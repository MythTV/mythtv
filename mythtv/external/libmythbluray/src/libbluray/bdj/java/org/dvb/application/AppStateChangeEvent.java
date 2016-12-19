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

import java.util.EventObject;

public class AppStateChangeEvent extends EventObject {
    public AppStateChangeEvent(AppID appid, int fromstate, int tostate,
            Object source, boolean hasFailed) {
        super(source);

        this.appid = appid;
        this.fromstate = fromstate;
        this.tostate = tostate;
        this.hasFailed = hasFailed;
    }

    public AppID getAppID() {
        return appid;
    }

    public int getFromState() {
        return fromstate;
    }

    public int getToState() {
        return tostate;
    }

    public boolean hasFailed() {
        return hasFailed;
    }

    public String toString() {
        return getClass().getName() + "[source=" + source + ",appid=" + appid + ",fromstate=" + fromstate + ",tostate=" + tostate + ",hasFailed=" + hasFailed + "]";
    }

    private final AppID appid;
    private final int fromstate;
    private final int tostate;
    private final boolean hasFailed;
    private static final long serialVersionUID = -5634352176873439145L;
}
