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

package org.videolan.media.content.audio;

import java.io.IOException;

import javax.media.Control;
import javax.media.ControllerErrorEvent;
import javax.media.IncompatibleSourceException;
import javax.media.Time;
import javax.media.protocol.DataSource;

import org.videolan.media.content.control.MediaTimePositionControlImpl;
import org.videolan.media.content.control.OverallGainControlImpl;
import org.videolan.media.content.control.PanningControlImpl;

import org.videolan.media.content.BDHandler;


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
        /*
          can be ex. file:/mnt/bluray/BDMV/JAR/00000/Sub_switch.pcm

        try {
            locator = new BDLocator(source.getLocator().toExternalForm());
        } catch (org.davic.net.InvalidLocatorException e) {
            throw new IncompatibleSourceException();
        }
        */
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
        return super.doStart(at);
    }

    /*
    protected ControllerErrorEvent doStop(boolean eof) {
        return super.doStop(eof);
    }
    */
    /*
    protected BDLocator getLocator() {
        return locator;
    }

    private BDLocator locator;
    */
    private org.videolan.media.protocol.dripfeed.DataSource source = null;
}
