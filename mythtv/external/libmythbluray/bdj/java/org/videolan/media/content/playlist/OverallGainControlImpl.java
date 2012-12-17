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
import javax.media.GainChangeListener;
import org.bluray.media.OverallGainControl;

public class OverallGainControlImpl implements OverallGainControl {
    public void setMute(boolean mute) {
        // TODO Not implemented
    }

    public boolean getMute() {
        return false; // TODO Not implemented
    }

    public float setDB(float gain) {
        return 0;  // TODO Not implemented
    }

    public float getDB() {
        return 0;  // TODO Not implemented
    }

    public float setLevel(float level) {
        return 0;
    }

    public float getLevel() {
        return 0;  // TODO Not implemented
    }

    public void addGainChangeListener(GainChangeListener listener) {
        // TODO Not implemented
    }

    public void removeGainChangeListener(GainChangeListener listener) {
        // TODO Not implemented
    }

    public Component getControlComponent() {
        return null;
    }

}
