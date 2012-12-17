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

public abstract interface AngleControl extends Control {
    public abstract int getCurrentAngle();

    public abstract int getAvailableAngles();

    public abstract void selectDefaultAngle();

    public abstract void selectAngle(int angle) throws InvalidAngleException;

    public abstract void addAngleChangeListener(AngleChangeListener listener);

    public abstract void removeAngleChangeListener(AngleChangeListener listener);
}
