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

import javax.media.Control;

public abstract interface PanningControl extends Control {
    public abstract void addPanningChangeListener(PanningChangeListener listener);

    public abstract void removePanningChangeListener(
            PanningChangeListener listener);

    public abstract float getLeftRight();

    public abstract float getFrontRear();

    public abstract void setLeftRight(float balance);

    public abstract void setFrontRear(float fade);

    public abstract void setPosition(float x, float y);
}
