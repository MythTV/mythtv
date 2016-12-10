/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import org.bluray.media.UOMaskTableChangedEvent;
import org.bluray.media.UOMaskedEvent;

import org.videolan.BDJListeners;
import org.videolan.Libbluray;
import org.videolan.PlaylistInfo;

public class UOMaskTableControlImpl implements UOMaskTableControl {
    protected UOMaskTableControlImpl(Handler player) {
    }

    public Component getControlComponent() {
        return null;
    }

    public void addUOMaskTableEventListener(UOMaskTableListener listener) {
        listeners.add(listener);
    }

    public void removeUOMaskTableEventListener(UOMaskTableListener listener) {
        listeners.remove(listener);
    }

    public boolean[] getMaskedUOTable() {
        long mask = Libbluray.getUOMask();
        boolean[] table = new boolean[64];
        for (int i = 0; i < table.length; i++)
            if (0L != (mask & 1L << i))
                table[i] = true;
            else
                table[i] = false;

        return table;
    }

    protected void onUOMasked(int position) {
        listeners.putCallback(new UOMaskedEvent(this, position));
    }

    protected void onPlayItemReach(int param) {
        // TODO: check if masked UO table actually changed
        listeners.putCallback(new UOMaskTableChangedEvent(this));
    }

    private BDJListeners listeners = new BDJListeners();
}
