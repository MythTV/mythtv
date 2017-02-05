/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2015  Petri Hintukainen
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

package org.videolan.media.content.control;

import java.awt.Component;

import org.bluray.media.PanningChangeEvent;
import org.bluray.media.PanningChangeListener;
import org.bluray.media.PanningControl;

import org.videolan.BDJListeners;
import org.videolan.media.content.BDHandler;

/*
  set audio source location in 2D space
*/
public class PanningControlImpl implements PanningControl {
    public PanningControlImpl(BDHandler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public void addPanningChangeListener(PanningChangeListener listener) {
        listeners.add(listener);
    }

    public void removePanningChangeListener(PanningChangeListener listener) {
        listeners.remove(listener);
    }

    public float getLeftRight() {
        return balance;
    }

    public float getFrontRear() {
        return fading;
    }

    public void setLeftRight(float balance) {
        setPosition(balance, this.fading);
    }

    public void setFrontRear(float panning) {
        setPosition(this.balance, panning);
    }

    public void setPosition(float x, float y) {
        this.balance = clip(x);
        this.fading = clip(y);

        player.setPanning(balance, fading);

        listeners.putCallback(new PanningChangeEvent(this, this.balance, this.fading));
    }

    private float clip(float val) {
        if (val != val) /* NaN */
            return 0.0f;
        return Math.min(-1.0f, Math.max(1.0f, val));
    }

    private BDHandler player;
    private BDJListeners listeners = new BDJListeners();
    private float fading = 0.0f;
    private float balance = 0.0f;
}
