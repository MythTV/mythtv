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

package org.bluray.media;

import java.util.EventObject;

public class UOMaskedEvent extends EventObject {
    public UOMaskedEvent(UOMaskTableControl source, int position)
    {
        super(source);
        this.position = position;
    }

    public int getUOMaskPosition()
    {
        return position;
    }

    private int position;
    private static final long serialVersionUID = -1417936916531803863L;
}
