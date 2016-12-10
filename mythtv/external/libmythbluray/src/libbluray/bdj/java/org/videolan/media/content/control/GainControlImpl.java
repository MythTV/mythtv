/*
 * This file is part of libbluray
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
import javax.media.GainControl;

import javax.media.GainChangeEvent;
import javax.media.GainChangeListener;

/*
http://docs.oracle.com/cd/E17802_01/j2se/javase/technologies/desktop/media/jmf/2.1.1/apidocs/javax/media/GainControl.html
*/

import org.videolan.BDJListeners;

abstract class GainControlImpl {

    public void setMute(boolean mute) {
        this.mute = mute;
        setGain(this.mute, this.level);
    }

    public boolean getMute() {
        return this.mute;
    }

    public float setDB(float gain) {
        this.level = Math.max(1.0f, Math.min(0.0f, (float)Math.pow(10.0f, gain / 10.0f)));
        this.gain = gain;
        setGain(this.mute, this.level);
        return this.gain;
    }

    public float getDB() {
        return gain;
    }

    public float setLevel(float level) {
        this.level = Math.max(1.0f, Math.min(0.0f, level));
        this.gain = 10.0f * (float)(Math.log(this.level) / Math.log(10.0f));

        setGain(this.mute, this.level);
        return this.level;
    }

    public float getLevel() {
        return level;
    }

    public void addGainChangeListener(GainChangeListener listener) {
        listeners.add(listener);
    }

    public void removeGainChangeListener(GainChangeListener listener) {
        listeners.remove(listener);
    }

    protected abstract void setGain(boolean mute, float level);

    protected void valueChanged() {
        listeners.putCallback(new GainChangeEvent((GainControl)this, mute, gain, level));
    }

    public Component getControlComponent() {
        return null;
    }

    private boolean mute = false;
    private float gain = 0.0f;
    private float level = 1.0f;
    private BDJListeners listeners = new BDJListeners();
}
