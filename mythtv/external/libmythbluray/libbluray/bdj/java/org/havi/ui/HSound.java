/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.havi.ui;

import java.io.IOException;
import java.net.URL;

import javax.media.ControllerEvent;
import javax.media.ControllerListener;
import javax.media.EndOfMediaEvent;
import javax.media.Manager;
import javax.media.NoPlayerException;
import javax.media.Player;
import javax.media.Time;

import org.videolan.Logger;

public class HSound {
    boolean isLooping = false;
    Player player = null;
    ControllerListenerImpl listener;

    public HSound() {
        listener = new ControllerListenerImpl();
    }

    public void load(String location) throws IOException, SecurityException {
        load(new URL("file:" + location));
    }

    public void load(URL contents) throws IOException, SecurityException {
        dispose();
        try {
            player = Manager.createPlayer(contents);
        } catch (NoPlayerException e) {
            IOException ioe = new IOException();
            ioe.initCause(e);
            throw ioe;
        }
    }

    public void set(byte data[]) {
        logger.unimplemented("set");
    }

    public void play() {
        if (player != null) {
            isLooping = false;
            player.addControllerListener(listener);
            player.start();
        }
    }

    public void stop() {
        if (player != null) {
            player.removeControllerListener(listener);
            player.stop();
        }
    }

    public void loop() {
        if (player != null) {
            isLooping = true;
            player.removeControllerListener(listener);
            player.start();
        }
    }

    public void dispose() {
        if (player != null) {
            player.removeControllerListener(listener);
            player.stop();
            player.close();
            player = null;
        }
    }


    private class ControllerListenerImpl implements ControllerListener {
        private ControllerListenerImpl() {
        }

        public void controllerUpdate(ControllerEvent event) {
            if (event instanceof EndOfMediaEvent) {
                if (isLooping) {
                    player.setMediaTime(new Time(0L));
                    player.start();
                } else {
                    stop();
                }
            }
        }
    }

    private static final Logger logger = Logger.getLogger(HSound.class.getName());
}
