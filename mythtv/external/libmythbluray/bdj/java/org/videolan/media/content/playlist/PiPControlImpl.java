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
import java.util.LinkedList;

import javax.media.Time;

import org.bluray.media.AsynchronousPiPControl;
import org.bluray.media.PiPControl;
import org.bluray.media.PiPStatusEvent;
import org.bluray.media.PiPStatusListener;
import org.bluray.media.StreamNotAvailableException;
import org.havi.ui.HScreenRectangle;

import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.Libbluray;
import org.videolan.StreamInfo;
import org.videolan.TIClip;

public class PiPControlImpl extends VideoControl implements PiPControl, AsynchronousPiPControl {
    protected PiPControlImpl(Handler player) {
        super(player, 1);
    }

    protected StreamInfo[] getStreams() {
        TIClip ci = player.getCurrentClipInfo();
        if (ci == null)
            return null;
        return ci.getSecVideoStreams();
    }

    protected void setStreamNumber(int num) {
        int psr = Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO)& 0xFFFF00FF;
        Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, psr | (num << 8));
    }

    public int getCurrentStreamNumber() {
        return (Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO) & 0x0000FF00) >> 8;
    }

    public void setDisplay(boolean value) {
        int psr = Libbluray.readPSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO) & 0x7FFFFFFF;
        if (value)
            psr |= 0x80000000;
        Libbluray.writePSR(Libbluray.PSR_SECONDARY_AUDIO_VIDEO, psr);
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
        throw new Error("Not implemented"); // TODO implement
    }

    protected void onPiPChange(int param) {
        synchronized (listeners) {
            if (!listeners.isEmpty())
                BDJActionManager.getInstance().putCallback(
                                                           new PiPCallback(this, param > 0));
        }
    }

    public void addPiPStatusListener(PiPStatusListener listener) {
        synchronized(listeners) {
            listeners.add(listener);
        }
    }

    public void removePiPStatusListener(PiPStatusListener listener) {
        synchronized(listeners) {
            listeners.remove(listener);
        }
    }

    private class PiPCallback extends BDJAction {
        private PiPCallback(PiPControlImpl control, boolean available) {
            this.control = control;
            this.available = available;
        }

        protected void doAction() {
            LinkedList list;
            synchronized (control.listeners) {
                list = (LinkedList)control.listeners.clone();
            }
            PiPStatusEvent event = new PiPStatusEvent(available, control);
            for (int i = 0; i < list.size(); i++)
                ((PiPStatusListener)list.get(i)).piPStatusChange(event);
        }

        private PiPControlImpl control;
        private boolean available;
    }

    public void start() {
        throw new Error("Not implemented"); // TODO implement
    }

    public void stop() {
        throw new Error("Not implemented"); // TODO implement
    }

    public void pause() {
        throw new Error("Not implemented"); // TODO implement
    }

    public boolean resume() {
        throw new Error("Not implemented"); // TODO implement
    }

    public Time getElapsedTime() {
        throw new Error("Not implemented"); // TODO implement
    }

    private LinkedList listeners = new LinkedList();
}
