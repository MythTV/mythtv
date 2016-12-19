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

package org.bluray.ti;

import java.util.LinkedList;

import org.videolan.BDJListeners;

public class DiscManager {

    private static DiscManager instance;
    private static final Object instanceLock = new Object();

    public static DiscManager getDiscManager() {
        synchronized (instanceLock) {
            if (instance == null)
                instance = new DiscManager();
            return instance;
        }
    }

    public void expectNextDisc(String[] discIds) {
        org.videolan.Logger.unimplemented(DiscManager.class.getName(), "expectNextDisc");
    }

    public Disc getCurrentDisc() {
        return new DiscImpl(discId);
    }

    public void addDiscStatusEventListener(DiscStatusListener listener) {
        listeners.add(listener);
    }

    public void removeDiscStatusEventListener(DiscStatusListener listener) {
        listeners.remove(listener);
    }

    // XXX not allowed to add new public methods -- should be done in Impl class
    public void setCurrentDisc(String id) {
        if (org.videolan.BDJXletContext.getCurrentContext() == null) {
            discId = id;
        } else {
            System.err.println("Ignoring disc ID (from Xlet)");
        }
    }

    private BDJListeners listeners = new BDJListeners();
    private String discId = null;
}
