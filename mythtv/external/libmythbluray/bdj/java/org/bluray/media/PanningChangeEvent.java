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

public class PanningChangeEvent extends EventObject {
    public PanningChangeEvent(PanningControl source, float balance, float fade)
    {
        super(source);

        this.balance = balance;
        this.fade = fade;
    }

    public float getFrontRear()
    {
        return fade;
    }

    public float getLeftRight()
    {
        return balance;
    }

    private float balance;
    private float fade;
    private static final long serialVersionUID = 75180748900043421L;
}
