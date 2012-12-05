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

package org.videolan.media.content.playlist;

import java.awt.Component;

import org.bluray.media.PanningChangeListener;
import org.bluray.media.PanningControl;

// TODO Figure out why the hell this is needed
public class PanningControlImpl implements PanningControl {
    public Component getControlComponent() {
        return null;
    }

    public void addPanningChangeListener(PanningChangeListener listener) {
        // TODO Not implemented
    }

    public void removePanningChangeListener(PanningChangeListener listener) {
        // TODO Not implemented
    }

    public float getLeftRight() {
        return 0;  // TODO Not implemented
    }

    public float getFrontRear() {
        return 0;  // TODO Not implemented
    }

    public void setLeftRight(float balance) {
        // TODO Not implemented
    }

    public void setFrontRear(float fade) {
        // TODO Not implemented
    }

    public void setPosition(float x, float y) {
        // TODO Not implemented
    }

}
