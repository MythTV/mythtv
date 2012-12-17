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

package org.havi.ui.event;

import java.util.EventObject;

public class HBackgroundImageEvent extends EventObject {
    public HBackgroundImageEvent(Object source, int id)
    {
        super(source);

        this.source = source;
        this.id = id;
    }

    public Object getSource()
    {
        return source;
    }

    public int getID()
    {
        return id;
    }

    public static final int BACKGROUNDIMAGE_FIRST = 1;
    public static final int BACKGROUNDIMAGE_LOADED = 1;
    public static final int BACKGROUNDIMAGE_FILE_NOT_FOUND = 2;
    public static final int BACKGROUNDIMAGE_IOERROR = 3;
    public static final int BACKGROUNDIMAGE_INVALID = 4;
    public static final int BACKGROUNDIMAGE_LAST = 4;

    private Object source;
    private int id;
    private static final long serialVersionUID = 4941828555539092236L;
}
