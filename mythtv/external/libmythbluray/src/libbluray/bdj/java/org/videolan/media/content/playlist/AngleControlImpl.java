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

import org.bluray.media.AngleChangeEvent;
import org.bluray.media.AngleChangeListener;
import org.bluray.media.AngleControl;
import org.bluray.media.InvalidAngleException;
import org.videolan.BDJListeners;
import org.videolan.Libbluray;
import org.videolan.PlaylistInfo;

public class AngleControlImpl implements AngleControl {
    protected AngleControlImpl(Handler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public int getCurrentAngle() {
        return Libbluray.getCurrentAngle();
    }

    public int getAvailableAngles() {
        PlaylistInfo pi = player.getPlaylistInfo();
        if (pi == null)
            return -1;
        return pi.getAngleCount();
    }

    public void selectDefaultAngle() {
        Libbluray.selectAngle(1);
    }

    public void selectAngle(int angle) throws InvalidAngleException {
        if (!Libbluray.selectAngle(angle))
            throw new InvalidAngleException();
    }

    public void addAngleChangeListener(AngleChangeListener listener) {
        listeners.add(listener);
    }

    public void removeAngleChangeListener(AngleChangeListener listener) {
        listeners.remove(listener);
    }

    protected void onAngleChange(int angle) {
        listeners.putCallback(new AngleChangeEvent(this, angle));
    }

    private BDJListeners listeners = new BDJListeners();
    private Handler player;
}
