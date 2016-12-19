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

package org.videolan.media.content.sound;

import java.io.IOException;

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

import org.bluray.net.BDLocator;

import org.videolan.media.content.control.MediaTimePositionControlImpl;
import org.videolan.media.content.control.OverallGainControlImpl;
import org.videolan.media.content.control.PanningControlImpl;

import org.videolan.media.content.BDHandler;

import org.videolan.BDJListeners;
import org.videolan.Libbluray;

public class Handler extends BDHandler {
    public Handler() {
        controls = new Control[3];
        controls[0] = new MediaTimePositionControlImpl(this);
        controls[1] = new OverallGainControlImpl(this);
        controls[2] = new PanningControlImpl(this);
    }

    public void setSource(DataSource source) throws IOException, IncompatibleSourceException {
        this.source = new org.videolan.media.protocol.dripfeed.DataSource(source.getLocator());
        if (source.getLocator() == null)
            throw new IncompatibleSourceException();

            try {
                locator = new BDLocator(source.getLocator().toExternalForm());
            } catch (org.davic.net.InvalidLocatorException e) {
                throw new IncompatibleSourceException();
            }
    }

    public Time getDuration() {
        org.videolan.Logger.unimplemented("Handler", "getDuration");
        long duration = 1; // pi.getDuration() ;
        return new Time(duration * TO_SECONDS);
    }

    protected ControllerErrorEvent doPrefetch() {
        return super.doPrefetch();
    }

    protected ControllerErrorEvent doStart(Time at) {

        ControllerErrorEvent err = super.doStart(at);
        if (err != null) {
            return err;
        }

        if (!locator.isSoundItem()) {
            System.err.println("no sound effect in " + locator);
        } else {
            int id = locator.getSoundId();
            Libbluray.soundEffect(id);

            // Trigger end of media event
            // XXX should use some other event name ...
            statusEvent(Libbluray.BDJ_EVENT_END_OF_PLAYLIST, id);
        }
        return null;
    }

    /*
    protected ControllerErrorEvent doStop(boolean eof) {
        return super.doStop(eof);
    }
    */

    protected BDLocator getLocator() {
        return locator;
    }

    private BDLocator locator;
    private org.videolan.media.protocol.dripfeed.DataSource source = null;
}
