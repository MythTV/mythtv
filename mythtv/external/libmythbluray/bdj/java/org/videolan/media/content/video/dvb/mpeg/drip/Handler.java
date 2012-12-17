/*
 * This file is part of libbluray
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

package org.videolan.media.content.video.dvb.mpeg.drip;

import java.awt.Component;
import java.io.IOException;
import java.util.ArrayList;

import javax.media.ClockStartedError;
import javax.media.ClockStoppedException;
import javax.media.ConnectionErrorEvent;
import javax.media.Control;
import javax.media.Controller;
import javax.media.ControllerErrorEvent;
import javax.media.ControllerListener;
import javax.media.GainControl;
import javax.media.IncompatibleSourceException;
import javax.media.IncompatibleTimeBaseException;
import javax.media.Player;
import javax.media.Time;
import javax.media.TimeBase;
import javax.media.protocol.DataSource;

//import org.videolan.media.content.playlist.BackgroundVideoPresentationControlImpl;

public class Handler implements Player {
    public Handler() {
        controls = new Control[1];
        controls[0] = new BackgroundVideoPresentationControlImpl(this);
    }

    public void setSource(DataSource source) throws IOException, IncompatibleSourceException {
        this.source = new org.videolan.media.protocol.dripfeed.DataSource(source.getLocator());
        if (source.getLocator() == null)
            throw new IncompatibleSourceException();
    }

    public int getState() {
        synchronized (this) {
            return state;
        }
    }

    public int getTargetState() {
        synchronized (this) {
            return targetState;
        }
    }

    public Time getStartLatency() {
        return null;
    }

    public Control[] getControls() {
        return controls;
    }

    public Control getControl(String forName) {
        try {
            Class cls = Class.forName(forName);
            for (int i = 0; i < controls.length; i++) {
                if (cls.isInstance(controls[i]))
                    return controls[i];
            }
            return null;
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    public void addControllerListener(ControllerListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    public void removeControllerListener(ControllerListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    public void setTimeBase(TimeBase master)
        throws IncompatibleTimeBaseException {
        throw new IncompatibleTimeBaseException();
    }

    public void realize() {
        // TODO Auto-generated method stub
    }

    public void prefetch() {
        // TODO Auto-generated method stub
    }

    public void start() {
        // TODO Auto-generated method stub
    }

    public void syncStart(Time at) {
        // TODO Auto-generated method stub
    }

    public void deallocate() {
        // TODO Auto-generated method stub
    }

    public void close() {
        // TODO Auto-generated method stub
    }

    public void stop() {
        // TODO Auto-generated method stub
    }

    public void setStopTime(Time stopTime) {
    }

    public Time getStopTime() {
        return null;
    }

    public void setMediaTime(Time now) {
    }

    public Time getMediaTime() {
        return new Time(0);
    }

    public long getMediaNanoseconds() {
        return 0;
    }

    public Time getSyncTime() {
        return null;
    }

    public TimeBase getTimeBase() {
        return null;
    }

    public Time mapToTimeBase(Time t) throws ClockStoppedException {
        return null;
    }

    public float getRate() {
        return 1.0f;
    }

    public float setRate(float factor) {
        return 1.0f;
    }

    public Component getVisualComponent() {
        return null;
    }

    public GainControl getGainControl() {
        return null;
    }

    public Component getControlPanelComponent() {
        return null;
    }

    public void addController(Controller newController)
        throws IncompatibleTimeBaseException {
    }

    public void removeController(Controller oldController) {
    }

    public Time getDuration() {
        return null;
    }

    protected int state = Unrealized;
    protected int targetState = Unrealized;
    protected Control[] controls = null;
    private org.videolan.media.protocol.dripfeed.DataSource source = null;
    private ArrayList listeners = new ArrayList();
}
