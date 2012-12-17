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

import org.bluray.media.UOMaskTableControl;
import org.bluray.media.UOMaskTableListener;
import org.videolan.PlaylistInfo;

// TODO: don't know what this is for
public class UOMaskTableControlImpl implements UOMaskTableControl {
    protected UOMaskTableControlImpl(Handler player) {
        this.player = player;
    }

    public Component getControlComponent() {
        return null;
    }

    public void addUOMaskTableEventListener(UOMaskTableListener listener) {
        throw new Error("Not implemented"); // TODO: Not implemented
    }

    public void removeUOMaskTableEventListener(UOMaskTableListener listener) {
        throw new Error("Not implemented"); // TODO: Not implemented
    }

    public boolean[] getMaskedUOTable() {
        PlaylistInfo pi = player.getPlaylistInfo();
        if (pi == null)
                return new boolean[0];
        boolean[] table = new boolean[64];
        for (int i = 0; i < 64; i++)
                table[i] = false;
        return table;
    }

    private Handler player;
}
