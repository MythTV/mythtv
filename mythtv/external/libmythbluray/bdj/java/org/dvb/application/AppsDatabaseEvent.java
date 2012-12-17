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

public class AppsDatabaseEvent extends EventObject {
    public AppsDatabaseEvent(int id, AppID appid, Object source)
    {
        super(source);

        this.appid = appid;
        this.id = id;
    }

    public AppID getAppID()
    {
        return appid;
    }

    public int getEventId()
    {
        return id;
    }

    static public final int NEW_DATABASE = 0;
    static public final int APP_CHANGED = 1;
    static public final int APP_ADDED = 2;
    static public final int APP_DELETED = 3;

    private AppID appid;
    private int id;
    private static final long serialVersionUID = 6941071319022607461L;
}
