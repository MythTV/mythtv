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

public class DiscManager {
    public static synchronized DiscManager getDiscManager() {
        if (instance == null)
            instance = new DiscManager();
        return instance;
    }

    public void expectNextDisc(String[] discIds) {
        throw new Error("Not implemented");
    }

    public Disc getCurrentDisc() {
        return disc;
    }

    public void addDiscStatusEventListener(DiscStatusListener listener) {
        throw new Error("Not implemented");
    }

    public void removeDiscStatusEventListener(DiscStatusListener listener) {
        throw new Error("Not implemented");
    }

    public void setCurrentDisc(String id) {
        disc = new DiscImpl(id);
    }

    private static DiscManager instance;
    private DiscImpl disc = null;
}
