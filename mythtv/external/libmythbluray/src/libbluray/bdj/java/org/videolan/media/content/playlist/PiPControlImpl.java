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

import java.awt.Dimension;

import javax.media.Time;

import org.bluray.media.AsynchronousPiPControl;
import org.bluray.media.PiPControl;
import org.bluray.media.PiPStatusEvent;
import org.bluray.media.PiPStatusListener;
import org.bluray.media.StreamNotAvailableException;
import org.havi.ui.HScreenRectangle;

import org.videolan.BDJListeners;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class PiPControlImpl extends VideoControl implements PiPControl, AsynchronousPiPControl {
    protected PiPControlImpl(Handler player) {
        super(1);
        this.player = player;
    }

    protected StreamInfo[] getStreams() {
        TIClip ci = player.getCurrentClipInfo();
        if (ci == null)
            return null;
        return ci.getSecVideoStreams();
    }

    protected void setStreamNumber(int num) {
        Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, num << 8, 0x0000ff00);
    }

    public int getCurrentStreamNumber() {
        return (Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO) & 0x0000FF00) >> 8;
    }

    public void setDisplay(boolean value) {
        Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, value ? 0x80000000 : 0, 0x80000000);
    }

    public boolean getDisplay() {
        return (Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO) & 0x80000000) != 0;
    }

    public void setFullScreen(boolean value) {
        if (value) {
            setVideoArea(new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f));
        } else {
            Dimension vd = getInputVideoSize();
            Dimension sd = getScreenSize();
            setVideoArea(new HScreenRectangle(0.5f, 0.5f, vd.width / sd.width, vd.height / sd.height));
        }
    }

    public boolean getFullScreen() {
        HScreenRectangle dr = getActiveVideoArea();
        return ((dr.x == 0.0f) && (dr.y == 0.0f) && (dr.width == 1.0f) && (dr.height == 1.0f));
    }

    public boolean getIsSyncedDuringTrickPlay() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "getIsSyncedDuringTrickPlay");
        return false;
    }

    protected void onPiPStatusChange(boolean enable) {
        listeners.putCallback(new PiPStatusEvent(enable, this));
    }

    public void addPiPStatusListener(PiPStatusListener listener) {
        listeners.add(listener);
    }

    public void removePiPStatusListener(PiPStatusListener listener) {
        listeners.remove(listener);
    }

    public void start() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "start");
    }

    public void stop() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "stop");
    }

    public void pause() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "pause");
    }

    public boolean resume() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "resume");
        return true;
    }

    public Time getElapsedTime() {
        org.videolan.Logger.unimplemented("PiPControlImpl", "getElapsedTime");
        return null;
    }

    private Handler player;
    private BDJListeners listeners = new BDJListeners();
}
